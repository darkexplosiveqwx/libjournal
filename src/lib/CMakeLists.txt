# SPDX-License-Identifier: BSD-3-Clause

set(JOURNAL_SOURCES
    journal.c
    transport.c
    encode.c
    util.c
)

add_library(journal_obj OBJECT ${JOURNAL_SOURCES})
set_target_properties(journal_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(journal_obj PRIVATE
    LIBJOURNAL_VERSION_STRING="${PROJECT_VERSION}"
    LIBJOURNAL_VERSION_MAJOR="${PROJECT_VERSION_MAJOR}"
    LIBJOURNAL_VERSION_MINOR="${PROJECT_VERSION_MINOR}"
    LIBJOURNAL_VERSION_PATCH="${PROJECT_VERSION_PATCH}"
)
target_include_directories(journal_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(JOURNAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

add_library(journal SHARED $<TARGET_OBJECTS:journal_obj>)
add_library(journal_static STATIC $<TARGET_OBJECTS:journal_obj>)

foreach(tgt journal journal_static)
    set_target_properties(${tgt} PROPERTIES OUTPUT_NAME journal)
    target_include_directories(${tgt} PUBLIC ${JOURNAL_INCLUDE_DIRS})
    target_compile_definitions(${tgt} PUBLIC
        LIBJOURNAL_VERSION_STRING="${PROJECT_VERSION}"
        LIBJOURNAL_VERSION_MAJOR="${PROJECT_VERSION_MAJOR}"
        LIBJOURNAL_VERSION_MINOR="${PROJECT_VERSION_MINOR}"
        LIBJOURNAL_VERSION_PATCH="${PROJECT_VERSION_PATCH}"
    )
    target_link_libraries(${tgt} PRIVATE pthread)
endforeach()

set_target_properties(journal PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
