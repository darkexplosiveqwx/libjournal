// SPDX-License-Identifier: BSD-3-Clause

#ifndef JOURNAL_H
#define JOURNAL_H

#include <sys/uio.h>
#include <sys/syslog.h>

int journal_init(void);

void journal_close(void);

int journal_get_fd(void);

int journal_print(int priority,
                  const char *format,
                  ...)
    __attribute__((format(printf, 2, 3)));

int journal_send(const char *format,
                 ...)
    __attribute__((format(printf, 1, 0)))
    __attribute__((sentinel));

int journal_sendv(const struct iovec *iov,
                  int n);

int journal_print_once(int priority,
                       const char *format,
                       ...)
    __attribute__((format(printf, 2, 3)));

int journal_send_once(const char *format,
                      ...)
    __attribute__((format(printf, 1, 0)))
    __attribute__((sentinel));

int journal_sendv_once(const struct iovec *iov,
                       int n);

#endif
