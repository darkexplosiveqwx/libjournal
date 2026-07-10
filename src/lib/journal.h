// SPDX-License-Identifier: BSD-3-Clause

#ifndef JOURNAL_H
#define JOURNAL_H

#include <sys/syslog.h>
#include <sys/uio.h>

int journal_init(void);

void journal_close(void);

int journal_get_fd(void) __attribute__((pure));

int journal_print(int priority, const char *format, ...) __attribute__((format(printf, 2, 3)));

int journal_send(const char *format, ...) __attribute__((format(printf, 1, 0)))
__attribute__((sentinel));

int journal_sendv(const struct iovec *iov, int n);

#endif
