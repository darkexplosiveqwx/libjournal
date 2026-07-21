# SPDX-License-Identifier: BSD-3-Clause

add_executable(test-journal
    test_journal.c
)

if(BUILD_SHARED_LIBS)
    target_link_libraries(test-journal PRIVATE journal)
else()
    target_link_libraries(test-journal PRIVATE journal_static)
endif()
target_compile_definitions(test-journal PRIVATE
    DEMO_BIN="$<TARGET_FILE:journal-demo>"
)

add_test(NAME test-journal COMMAND test-journal)
