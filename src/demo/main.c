// SPDX-License-Identifier: BSD-3-Clause

#include "journal.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void) {
    int r;

    r = journal_init();
    if (r < 0) {
        fprintf(stderr, "journal_init() failed: %s\n", strerror(-r));
        return 1;
    }

    journal_print(LOG_INFO, "Hello %s", "world");

    journal_send(
        "MESSAGE=Structured message",
        "PRIORITY=6",
        "USER=test",
        NULL);

    {
        const char *msg = "Manual iovec field";
        const char *prio = "PRIORITY=7\n";
        struct iovec iov[2];
        iov[0].iov_base = (void *)msg;
        iov[0].iov_len = strlen(msg) + 1;
        iov[1].iov_base = (void *)prio;
        iov[1].iov_len = strlen(prio);

        r = journal_sendv(iov, 2);
        if (r < 0)
            fprintf(stderr, "journal_sendv() failed: %s\n", strerror(-r));
    }

    journal_close();

    journal_print_once(LOG_ERR, "One-shot message: %d", 42);

    journal_send_once(
        "MESSAGE=One-shot structured",
        "PRIORITY=%i", LOG_WARNING,
        NULL);

    {
        const char *msg = "One-shot manual iovec\n";
        struct iovec iov[1];
        iov[0].iov_base = (void *)msg;
        iov[0].iov_len = strlen(msg);

        r = journal_sendv_once(iov, 1);
        if (r < 0)
            fprintf(stderr, "journal_sendv_once() failed: %s\n", strerror(-r));
    }

    fprintf(stderr, "Demo complete. If journald is not running, errors above are expected.\n");
    return 0;
}
