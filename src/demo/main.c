// SPDX-License-Identifier: BSD-3-Clause

#include "journal.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
	int r;

	r = journal_init();
	if (r < 0)
	{
		fprintf(stderr, "journal_init() failed: %s\n", strerror(-r));
		return 1;
	}

	r = journal_print(LOG_INFO, "Hello %s", "world");
	if (r < 0)
		fprintf(stderr, "journal_print() failed: %s\n", strerror(-r));

	r = journal_send("MESSAGE=Structured message", "PRIORITY=6", "USER=test", NULL);
	if (r < 0)
		fprintf(stderr, "journal_send() failed: %s\n", strerror(-r));

	// Field containing a newline triggers automatic binary encoding
	r = journal_send("BINARY_FIELD=hello\nworld", "PRIORITY=6", NULL);
	if (r < 0)
		fprintf(stderr, "journal_send() failed: %s\n", strerror(-r));

	// Equal signs in values must survive the key=value split
	r = journal_send("EQUAL_FIELD=a=FOO=c", "PRIORITY=6", NULL);
	if (r < 0)
		fprintf(stderr, "journal_send() failed: %s\n", strerror(-r));

	{
		const char *msg = "MESSAGE=Manual iovec field\n";
		const char *prio = "PRIORITY=7\n";
		struct iovec iov[2];
		iov[0].iov_base = (void *)msg;
		iov[0].iov_len = strlen(msg);
		iov[1].iov_base = (void *)prio;
		iov[1].iov_len = strlen(prio);

		r = journal_sendv(iov, 2);
		if (r < 0)
			fprintf(stderr, "journal_sendv() failed: %s\n", strerror(-r));
	}

	// Large message (>128 KiB) to exercise the memfd fallback path
	{
		size_t sz = 200 * 1024;
		char *buf = malloc(8 + sz + 1);
		if (buf)
		{
			memcpy(buf, "MESSAGE=", 8);
			memcpy(buf + 8, "LARGE_MEMFD_PAYLOAD:", 20);
			for (size_t i = 20; i < sz; i++)
				buf[8 + i] = 'A';
			buf[8 + sz] = '\n';

			const char *prio = "PRIORITY=6\n";
			struct iovec iov[2];
			iov[0].iov_base = buf;
			iov[0].iov_len = 8 + sz + 1;
			iov[1].iov_base = (void *)prio;
			iov[1].iov_len = strlen(prio);

			r = journal_sendv(iov, 2);
			if (r < 0)
				fprintf(stderr, "journal_sendv(large) failed: %s\n", strerror(-r));
			free(buf);
		}
	}

	// this is not necessary for regular use and usually only called so the file descriptor doesn't show up in valgrind/gdb.
	// Letting it leak is the recommended and simpler approach.
	journal_close();

	struct timespec ts = {
    .tv_sec = 0,
    .tv_nsec = 1000000
	};
	// sleep for 1ms to allow journald to generate more metadata, otherwise we will have exited by the time journald reads /proc with our PID
	nanosleep(&ts, NULL);

	fprintf(stderr, "Demo complete. If journald is not running, errors above are expected.\n");
	return 0;
}
