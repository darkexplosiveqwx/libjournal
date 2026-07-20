# SPDX-License-Identifier: BSD-3-Clause
cmake_minimum_required(VERSION 3.21)
set(CMAKE_C_STANDARD 17)

project(libjournal VERSION 0.2.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(BUILD_DEMO "Build demo application" ON)
option(BUILD_TESTS "Build protocol tests" ON)

if(BUILD_TESTS AND NOT BUILD_DEMO)
  message(FATAL_ERROR "BUILD_TESTS requires BUILD_DEMO to be ON")
endif()

include(GNUInstallDirs)
enable_testing()

add_subdirectory(src/lib)

if(BUILD_DEMO)
  add_subdirectory(src/demo)
endif()

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()

install(TARGETS journal
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(TARGETS journal_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES src/lib/journal.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_file(${CMAKE_SOURCE_DIR}/libjournal.pc.in
    ${CMAKE_BINARY_DIR}/libjournal.pc
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/libjournal.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
