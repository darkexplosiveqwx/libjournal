// SPDX-License-Identifier: BSD-3-Clause

#include "journal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	int r;

	r = journal_init();
	if (r < 0)
	{
		fprintf(stderr, "journal_init() failed: %s\n", strerror(-r));
		return 1;
	}

	journal_print(LOG_INFO, "Hello %s", "world");

	journal_send("MESSAGE=Structured message", "PRIORITY=6", "USER=test", NULL);

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

	// this is not necessary for regular use and usually only called so the file descriptor doesn't show up in valgrind/gdb.
	// Letting it leak is the recommended and simpler approach.
	journal_close();

	fprintf(stderr, "Demo complete. If journald is not running, errors above are expected.\n");
	return 0;
}
