# SPDX-License-Identifier: BSD-3-Clause

add_executable(journal-demo
    main.c
)

target_link_libraries(journal-demo PRIVATE journal)
