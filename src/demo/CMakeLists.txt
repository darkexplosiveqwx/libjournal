# SPDX-License-Identifier: BSD-3-Clause

add_executable(journal-demo
    main.c
)

if(BUILD_SHARED_LIBS)
    target_link_libraries(journal-demo PRIVATE journal)
else()
    target_link_libraries(journal-demo PRIVATE journal_static)
endif()
