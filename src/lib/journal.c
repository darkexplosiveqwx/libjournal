// SPDX-License-Identifier: BSD-3-Clause

#include "journal.h"

#include "encode.h"
#include "transport.h"
#include "util.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IOV 64
#define FIELD_BUF 4096
#define MAX_KEY 256
#define VALUE_BUF (MAX_IOV * (MAX_KEY + 1 + FIELD_BUF))

static pthread_key_t field_buf_key;
static pthread_once_t field_buf_once = PTHREAD_ONCE_INIT;

static void field_buf_destructor(void *p)
{
	free(p);
}

static int field_buf_key_ready;

static void field_buf_key_init(void)
{
	if (pthread_key_create(&field_buf_key, field_buf_destructor) == 0)
		field_buf_key_ready = 1;
}

static int send_impl(const char *format, va_list ap)
{
	if (!format)
		return -EINVAL;

	static _Thread_local struct iovec iov[MAX_IOV];
	static _Thread_local char *field_buf;
	if (!field_buf)
	{
		field_buf = malloc(VALUE_BUF);
		if (!field_buf)
			return -ENOMEM;
		pthread_once(&field_buf_once, field_buf_key_init);
		if (field_buf_key_ready)
			pthread_setspecific(field_buf_key, field_buf);
	}

	int n_iov = 0;
	int off = 0;

	const char *f = format;
	while (f && n_iov < MAX_IOV)
	{
		const char *eq = strchr(f, '=');
		if (!eq)
			break;

		const char *value_fmt = eq + 1;
		int n_args = util_count_printf_conversions(value_fmt);

		size_t key_len = (size_t)(eq - f);
		if (key_len == 0 || key_len > MAX_KEY)
		{
			for (int i = 0; i < n_args; i++)
				(void)va_arg(ap, void *);
			f = va_arg(ap, const char *);
			continue;
		}

		if (encode_validate_key(f, key_len) < 0)
		{
			for (int i = 0; i < n_args; i++)
				(void)va_arg(ap, void *);
			f = va_arg(ap, const char *);
			continue;
		}

		size_t value_max;
		if (n_args > 0)
		{
			value_max = FIELD_BUF;
		}
		else
		{
			value_max = strlen(value_fmt);
			if (value_max >= FIELD_BUF)
				value_max = FIELD_BUF - 1;
		}

		size_t need = key_len + 1 + value_max;
		if (off + (int)need > VALUE_BUF)
			break;

		char *buf = field_buf + off;
		memcpy(buf, f, key_len);
		buf[key_len] = '=';

		int value_len;
		if (n_args > 0)
		{
			va_list copy;
			va_copy(copy, ap);
			value_len = vsnprintf(buf + key_len + 1, FIELD_BUF, value_fmt, copy);
			va_end(copy);

			if (value_len < 0)
			{
				for (int i = 0; i < n_args; i++)
					(void)va_arg(ap, void *);
				f = va_arg(ap, const char *);
				continue;
			}
			if ((size_t)value_len >= FIELD_BUF)
				value_len = FIELD_BUF - 1;
		}
		else
		{
			memcpy(buf + key_len + 1, value_fmt, value_max);
			buf[key_len + 1 + value_max] = '\0';
			value_len = (int)value_max;
		}

		value_len = (int)encode_trim_trailing_whitespace(buf + key_len + 1, (size_t)value_len);

		iov[n_iov].iov_base = buf;
		iov[n_iov].iov_len = key_len + 1 + (size_t)value_len;
		n_iov++;
		off += (int)(key_len + 1 + (size_t)value_len);

		for (int i = 0; i < n_args; i++)
			(void)va_arg(ap, void *);
		f = va_arg(ap, const char *);
	}

	if (n_iov == 0)
		return -EINVAL;

	return transport_send(iov, n_iov);
}

int journal_init(void)
{
	return transport_init();
}

void journal_close(void)
{
	transport_close();
}

int journal_get_fd(void)
{
	return transport_get_fd();
}

int journal_print(int priority, const char *format, ...)
{
	if (!format)
		return -EINVAL;

	if (priority < 0 || priority > 7)
		return -EINVAL;

	char msg[FIELD_BUF];
	char msg_buf[64 + FIELD_BUF];
	char prio_val[16];
	char prio_buf[32];
	struct iovec iov[2];
	int n_iov = 0;

	va_list ap;
	va_start(ap, format);
	int msg_len = vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);

	if (msg_len < 0)
		return -EINVAL;
	if ((size_t)msg_len >= sizeof(msg))
		msg_len = (int)sizeof(msg) - 1;

	msg_len = (int)encode_trim_trailing_whitespace(msg, (size_t)msg_len);

	memcpy(msg_buf, "MESSAGE=", 8);
	memcpy(msg_buf + 8, msg, (size_t)msg_len);
	iov[n_iov].iov_base = msg_buf;
	iov[n_iov].iov_len = 8 + (size_t)msg_len;
	n_iov++;

	int prio_len = snprintf(prio_val, sizeof(prio_val), "%d", priority);
	if (prio_len < 0)
		return -EINVAL;

	memcpy(prio_buf, "PRIORITY=", 9);
	memcpy(prio_buf + 9, prio_val, (size_t)prio_len);
	iov[n_iov].iov_base = prio_buf;
	iov[n_iov].iov_len = 9 + (size_t)prio_len;
	n_iov++;

	return transport_send(iov, n_iov);
}

int journal_send(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int r = send_impl(format, ap);
	va_end(ap);
	return r;
}

int journal_sendv(const struct iovec *iov, int n)
{
	return transport_send(iov, n);
}
