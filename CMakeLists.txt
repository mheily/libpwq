#
# Copyright (c) 2011 Marius Zwicker <marius@mlba-team.de>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

cmake_minimum_required(VERSION 2.8.4)
cmake_policy(SET CMP0063 OLD)

project(pthread_workqueue C)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

option(STATIC_WORKQUEUE "build libpthread_workqueue as static library" OFF)
option(ENABLE_TESTS "build tests for libpthread_workqueue" OFF)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREADS_FLAG TRUE)
find_package(Threads REQUIRED)

include(CheckIncludeFiles)

if(ENABLE_TESTS)
  check_include_files(err.h HAVE_ERR_H)
  # HACK: Avoid generating a config.h for just a single definition
  if(HAVE_ERR_H)
    add_definitions(-DHAVE_ERR_H=1)
  else()
    add_definitions(-DHAVE_ERR_H=0)
  endif()
  add_definitions(-DNO_CONFIG_H)
endif()

set(PTHREAD_WORKQUEUE_HEADERS
    include/pthread_workqueue.h)

set(PTHREAD_WORKQUEUE_SOURCES
    src/api.c
    src/debug.h
    src/private.h
    src/thread_info.h
    src/thread_rt.h
    src/witem_cache.c)

if(CMAKE_SYSTEM_NAME MATCHES Windows)
  list(APPEND PTHREAD_WORKQUEUE_SOURCES
       src/windows/manager.c
       src/windows/platform.c
       src/windows/platform.h
       src/windows/posix_semaphore.h
       src/windows/pthread_cond.h
       src/windows/pthread_cond_variables.h
       src/windows/pthread_create.h
       src/windows/pthread_mutex.h
       src/windows/pthread_rw_lock.h
       src/windows/pthread_tls.h
       src/windows/queue.h
       src/windows/stdint.h
       src/windows/thread_info.c
       src/windows/thread_rt.c
       src/windows/threads.h
       src/windows/times.h)
elseif(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
  list(APPEND PTHREAD_WORKQUEUE_SOURCES
       src/posix/manager.c
       src/posix/platform.h
       src/posix/thread_info.c
       src/posix/thread_rt.c
       src/solaris/load.c
       src/solaris/thread_info.c
       src/solaris/thread_rt.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
  list(APPEND PTHREAD_WORKQUEUE_SOURCES
       src/posix/manager.c
       src/posix/platform.h
       src/posix/thread_info.c
       src/posix/thread_rt.c
       src/linux/load.c
       src/linux/thread_info.c
       src/linux/thread_rt.c)
else()
  message(FATAL_ERROR "unsupported host os")
endif()

source_group(includes
             FILES
               ${PTHREAD_WORKQUEUE_HEADERS})
source_group(src
             FILES
               ${PTHREAD_WORKQUEUE_SOURCES})

if(STATIC_WORKQUEUE)
  set(LIBRARY_TYPE STATIC)
else()
  set(LIBRARY_TYPE SHARED)
endif()

add_library(pthread_workqueue
            ${LIBRARY_TYPE}
              ${PTHREAD_WORKQUEUE_SOURCES}
              ${PTHREAD_WORKQUEUE_HEADERS})
set_target_properties(pthread_workqueue PROPERTIES DEBUG_POSTFIX "D")

if(WIN32)
  target_compile_definitions(pthread_workqueue PRIVATE
                             LIBPTHREAD_WORKQUEUE_EXPORTS;_USRDLL;_WINDLL)
else()
  target_compile_definitions(pthread_workqueue PRIVATE
                             _XOPEN_SOURCE=600;_GNU_SOURCE)
endif()
if(STATIC_WORKQUEUE)
  target_compile_definitions(pthread_workqueue PRIVATE MAKE_STATIC)
endif()

target_include_directories(pthread_workqueue PRIVATE include)
if(NOT WIN32)
  target_include_directories(pthread_workqueue PRIVATE src)
endif()

target_link_libraries(pthread_workqueue PUBLIC ${CMAKE_THREAD_LIBS_INIT})

if(ENABLE_TESTS)
  add_subdirectory(testing)
endif()

