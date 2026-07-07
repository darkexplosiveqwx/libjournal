# SPDX-License-Identifier: BSD-3-Clause

add_executable(test-journal
    test_journal.c
)

target_link_libraries(test-journal PRIVATE journal)
