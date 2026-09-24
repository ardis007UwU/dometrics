cmake_minimum_required(VERSION 3.16)
project(dometrics VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# --- Compiler hardening -------------------------------------------------------
# -Wshadow: catch variable shadowing; stack protector: harden stack frames;
# _FORTIFY_SOURCE: compile-time/checked libc buffer overflows (needs -O).
add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -fstack-protector-strong)
add_compile_options(
  -U_FORTIFY_SOURCE
  $<$<CONFIG:Release,RelWithDebInfo>:-D_FORTIFY_SOURCE=2>
  $<$<CONFIG:MinSizeRel>:-D_FORTIFY_SOURCE=2>
)

# Optional fully-static link (used by CI release builds / musl).
option(DOMETRICS_STATIC "Statically link the dometrics binary" OFF)

# --- Dependencies: SQLite3, Threads, Filesystem -------------------------------
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SQLITE3 REQUIRED sqlite3)
endif()

find_package(Threads REQUIRED)

# --- Sources -------------------------------------------------------------------
set(DOMETRICS_SOURCES
  src/main.cpp
  src/db/db_client.cpp
  src/daemon/watcher.cpp
  src/cli/cli_router.cpp
  src/core/git_utils.cpp
  src/core/loc_engine.cpp
  src/core/scanner.cpp
)

add_executable(dometrics ${DOMETRICS_SOURCES})

target_include_directories(dometrics PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/db
  ${CMAKE_CURRENT_SOURCE_DIR}/src/daemon
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cli
)

if(DOMETRICS_STATIC)
  # Fully-static release builds need the SQLite static archive; the shared
  # imported target cannot be used with -static.
  find_file(SQLITE3_STATIC_ARCHIVE NAMES libsqlite3.a)
  if(NOT SQLITE3_STATIC_ARCHIVE)
    message(FATAL_ERROR
      "DOMETRICS_STATIC=ON but libsqlite3.a was not found. "
      "Install the static SQLite archive (Debian/Ubuntu: libsqlite3-dev) "
      "or pass -DSQLITE3_STATIC_ARCHIVE=/path/to/libsqlite3.a")
  endif()
  message(STATUS "Static SQLite: ${SQLITE3_STATIC_ARCHIVE}")
  target_link_libraries(dometrics PRIVATE ${SQLITE3_STATIC_ARCHIVE} Threads::Threads)
  target_link_options(dometrics PRIVATE -static)
elseif(TARGET SQLite3::SQLite3)
  # Canonical imported target from FindSQLite3 (CMake >= 3.14).
  target_link_libraries(dometrics PRIVATE SQLite3::SQLite3 Threads::Threads)
elseif(TARGET SQLite::SQLite3)
  # Deprecated alias kept for older CMake revisions only.
  target_link_libraries(dometrics PRIVATE SQLite::SQLite3 Threads::Threads)
else()
  target_include_directories(dometrics PRIVATE ${SQLITE3_INCLUDE_DIRS})
  target_link_directories(dometrics PRIVATE ${SQLITE3_LIBRARY_DIRS})
  target_link_libraries(dometrics PRIVATE ${SQLITE3_LIBRARIES} Threads::Threads)
endif()

# stdc++fs is required on GCC < 9 for <filesystem>.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(dometrics PRIVATE stdc++fs)
endif()

# --- Installation (GNU standard directories) -----------------------------------
# `dometrics install` / distro packaging place the single CLI binary in BINDIR.
include(GNUInstallDirs)
install(TARGETS dometrics DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- CPack packaging (Debian .deb + RedHat .rpm) --------------------------------
# `cpack -G DEB` / `cpack -G RPM` (or bare `cpack`) produce distributable
# installers from this configuration; CI attaches both to GitHub Releases.
set(CPACK_PACKAGE_NAME "dometrics")
set(CPACK_PACKAGE_VENDOR "Dometrics Project")
set(CPACK_PACKAGE_CONTACT "ardis007UwU <ardis007@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Zero-maintenance developer telemetry CLI and background daemon")
set(CPACK_PACKAGE_DESCRIPTION
    "Dometrics tracks active coding time via filesystem events, computes git line-of-code deltas, and aggregates multi-repo lineages into a single SQLite WAL database.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ardis007UwU/dometrics")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "dometrics")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_STRIP_FILES ON)

# Debian
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# RedHat / Fedora / Copr
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_AUTOREQPROV "yes")

include(CPack)

# Helpful summary
message(STATUS "Dometrics v${PROJECT_VERSION} | build=${CMAKE_BUILD_TYPE} | static=${DOMETRICS_STATIC} | CXX=${CMAKE_CXX_COMPILER_ID}")
