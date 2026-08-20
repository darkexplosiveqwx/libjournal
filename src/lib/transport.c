// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "transport.h"
#include "encode.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_IOV 64

#define JOURNAL_SOCKET_PATH "/run/systemd/journal/socket"
#define JOURNAL_SNDBUF_SIZE (256U * 1024U)
#define JOURNAL_MAX_DGRAM (128U * 1024U)

static _Atomic int g_fd = -1;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

static int socket_create(void)
{
	int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;

	int sndbuf = JOURNAL_SNDBUF_SIZE;
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

	return fd;
}

static int socket_connect(int fd)
{
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	size_t path_len = sizeof(JOURNAL_SOCKET_PATH) - 1;
	if (path_len >= sizeof(addr.sun_path))
	{
		return -ENAMETOOLONG;
	}
	memcpy(addr.sun_path, JOURNAL_SOCKET_PATH, path_len);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		return -errno;

	return 0;
}

static int seal_memfd(int memfd)
{
	int seals = F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE;
	return fcntl(memfd, F_ADD_SEALS, seals);
}

static int transport_send_memfd(int fd, const struct iovec *iov, int iov_len, size_t total)
{
	int memfd = memfd_create("libjournal", MFD_ALLOW_SEALING | MFD_CLOEXEC);
	if (memfd < 0)
		return -errno;

	ssize_t written;
	do
	{
		written = writev(memfd, iov, iov_len);
	} while (written < 0 && errno == EINTR);

	if (written < 0 || (size_t)written != total)
	{
		int err = (written < 0) ? errno : EIO;
		close(memfd);
		return -err;
	}

	if (lseek(memfd, 0, SEEK_SET) == -1)
	{
		int err = errno;
		close(memfd);
		return -err;
	}

	int seal = seal_memfd(memfd);
	if (seal < 0)
	{
		close(memfd);
		return -seal;
	}

	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));

	msg.msg_iov = NULL;
	msg.msg_iovlen = 0;

	char cmsg_buf[CMSG_SPACE(sizeof(int))];
	msg.msg_control = cmsg_buf;
	msg.msg_controllen = sizeof(cmsg_buf);

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cmsg), &memfd, sizeof(memfd));
	msg.msg_controllen = cmsg->cmsg_len;

	ssize_t r;
	do
	{
		r = sendmsg(fd, &msg, MSG_NOSIGNAL);
	} while (r < 0 && errno == EINTR);

	int err = r < 0 ? errno : 0;
	close(memfd);
	return err ? -err : 0;
}

int transport_init(void)
{
	if (atomic_load_explicit(&g_fd, memory_order_acquire) >= 0)
		return 0;

	pthread_mutex_lock(&g_init_mutex);

	if (atomic_load_explicit(&g_fd, memory_order_relaxed) >= 0)
	{
		pthread_mutex_unlock(&g_init_mutex);
		return 0;
	}

	int fd = socket_create();
	if (fd < 0)
	{
		int err = errno;
		pthread_mutex_unlock(&g_init_mutex);
		return -err;
	}

	int r = socket_connect(fd);
	if (r < 0)
	{
		close(fd);
		pthread_mutex_unlock(&g_init_mutex);
		return r;
	}

	atomic_store_explicit(&g_fd, fd, memory_order_release);

	pthread_mutex_unlock(&g_init_mutex);
	return 0;
}

void transport_close(void)
{
	int fd = atomic_exchange_explicit(&g_fd, -1, memory_order_acq_rel);
	if (fd >= 0)
		close(fd);
}

int transport_get_fd(void)
{
	return atomic_load_explicit(&g_fd, memory_order_acquire);
}

int transport_send(const struct iovec *iov, int iov_len)
{
	if (iov_len < 0)
		return -EINVAL;
	if (iov_len == 0)
		return 0;
	if (!iov)
		return -EINVAL;

	static _Thread_local struct iovec out[MAX_IOV * 4];
	static _Thread_local unsigned char le_bufs[MAX_IOV][8];
	static _Thread_local char *key_bufs[MAX_IOV];
	static _Thread_local size_t key_buf_caps[MAX_IOV];
	static const char nl = '\n';
	static const char eq_char = '=';
	int n_out = 0;
	int n_fields = 0;

	for (int i = 0; i < iov_len && n_fields < MAX_IOV; i++)
	{
		const char *base = iov[i].iov_base;
		size_t len = iov[i].iov_len;
		if (!base || len == 0)
			continue;

		const char *eq = memchr(base, '=', len);
		if (!eq)
			continue;

		const char *key = base;
		size_t key_len = (size_t)(eq - base);
		const char *value = eq + 1;
		size_t value_len = len - key_len - 1;

		if (encode_validate_key(key, key_len) < 0)
			continue;

		if (encode_needs_binary(value, value_len))
		{
			size_t kb_need = key_len + 1;
			if (kb_need > key_buf_caps[n_fields])
			{
				char *tmp = realloc(key_bufs[n_fields], kb_need);
				if (!tmp)
					continue;
				key_bufs[n_fields] = tmp;
				key_buf_caps[n_fields] = kb_need;
			}

			int idx = n_out;
			int r = encode_binary(out, MAX_IOV * 4, &idx, key, key_len, value, value_len,
								  key_bufs[n_fields], le_bufs[n_fields]);
			if (r < 0)
				continue;
			n_out = idx;
		}
		else
		{
			if (n_out + 4 > MAX_IOV * 4)
				break;

			out[n_out].iov_base = (void *)key;
			out[n_out].iov_len = key_len;
			n_out++;

			out[n_out].iov_base = (void *)&eq_char;
			out[n_out].iov_len = 1;
			n_out++;

			out[n_out].iov_base = (void *)value;
			out[n_out].iov_len = value_len;
			n_out++;

			out[n_out].iov_base = (void *)&nl;
			out[n_out].iov_len = 1;
			n_out++;
		}

		n_fields++;
	}

	if (n_out == 0)
		return -EINVAL;

	size_t total = 0;
	for (int i = 0; i < n_out; i++)
		total += out[i].iov_len;

	int fd = atomic_load_explicit(&g_fd, memory_order_acquire);
	if (fd < 0)
		return -ENOTCONN;

	if (total > JOURNAL_MAX_DGRAM)
		return transport_send_memfd(fd, out, n_out, total);

	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = out;
	msg.msg_iovlen = n_out;

	ssize_t r;
	do
	{
		r = sendmsg(fd, &msg, MSG_NOSIGNAL);
	} while (r < 0 && errno == EINTR);

	if (r >= 0)
		return 0;

	int err = errno;

	if (err == EMSGSIZE)
		return transport_send_memfd(fd, out, n_out, total);

	if (err == ECONNREFUSED || err == ENOTCONN || err == EPIPE)
	{
		pthread_mutex_lock(&g_init_mutex);

		if (atomic_load_explicit(&g_fd, memory_order_relaxed) == fd)
		{
			close(fd);
			atomic_store_explicit(&g_fd, -1, memory_order_release);

			int new_fd = socket_create();
			if (new_fd >= 0)
			{
				int cr = socket_connect(new_fd);
				if (cr == 0)
				{
					atomic_store_explicit(&g_fd, new_fd, memory_order_release);
					fd = new_fd;
					err = 0;
				}
				else
				{
					err = -cr;
					close(new_fd);
				}
			}
			else
			{
				err = errno;
			}
		}
		else
		{
			fd = atomic_load_explicit(&g_fd, memory_order_relaxed);
			if (fd >= 0)
				err = 0;
		}

		pthread_mutex_unlock(&g_init_mutex);

		if (err)
			return -err;

		if (total > JOURNAL_MAX_DGRAM)
			return transport_send_memfd(fd, out, n_out, total);

		msg.msg_iov = out;
		msg.msg_iovlen = n_out;

		do
		{
			r = sendmsg(fd, &msg, MSG_NOSIGNAL);
		} while (r < 0 && errno == EINTR);

		if (r >= 0)
			return 0;

		err = errno;
		if (err == EMSGSIZE)
			return transport_send_memfd(fd, out, n_out, total);

		return -err;
	}

	return -err;
}
