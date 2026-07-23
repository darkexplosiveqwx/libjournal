# SPDX-License-Identifier: BSD-3-Clause

find_package(Threads REQUIRED)

set(JOURNAL_SOURCES
    journal.c
    transport.c
    encode.c
    util.c
)

add_library(journal_obj OBJECT ${JOURNAL_SOURCES})
set_target_properties(journal_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(journal_obj PRIVATE
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(JOURNAL_LIBS "")
if(BUILD_SHARED_ARTIFACT)
  add_library(journal SHARED $<TARGET_OBJECTS:journal_obj>)
  list(APPEND JOURNAL_LIBS journal)
endif()
if(BUILD_STATIC_ARTIFACT)
  add_library(journal_static STATIC $<TARGET_OBJECTS:journal_obj>)
  list(APPEND JOURNAL_LIBS journal_static)
endif()

foreach(tgt ${JOURNAL_LIBS})
    set_target_properties(${tgt} PROPERTIES OUTPUT_NAME journal)
    target_include_directories(${tgt} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_compile_definitions(${tgt} PUBLIC
        LIBJOURNAL_VERSION_STRING="${PROJECT_VERSION}"
        LIBJOURNAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        LIBJOURNAL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        LIBJOURNAL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    )
    target_link_libraries(${tgt} PRIVATE Threads::Threads)
endforeach()

if(BUILD_SHARED_ARTIFACT)
  set_target_properties(journal PROPERTIES
      VERSION   ${PROJECT_VERSION}
      SOVERSION ${PROJECT_VERSION_MAJOR}
  )
endif()
