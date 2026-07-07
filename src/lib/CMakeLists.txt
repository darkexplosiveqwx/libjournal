# SPDX-License-Identifier: BSD-3-Clause

set(JOURNAL_SOURCES
    journal.c
    transport.c
    encode.c
    util.c
)

add_library(journal ${JOURNAL_SOURCES})

target_include_directories(journal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(journal PRIVATE pthread)
