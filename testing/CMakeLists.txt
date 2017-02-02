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

function(ADD_UNIT_TEST TEST)
  set(options DISABLED_TEST)
  set(single_value_args)
  set(multiple_value_args SOURCES)
  cmake_parse_arguments(AUT "${options}" "${single_value_args}" "${multiple_value_args}" ${ARGN})

  if(AUT_DISABLED_TEST)
    return()
  endif()

  add_executable(test_${TEST}_pthread_workqueue ${AUT_SOURCES})
  target_include_directories(test_${TEST}_pthread_workqueue
                             PRIVATE
                               ${CMAKE_SOURCE_DIR}
                               ${CMAKE_SOURCE_DIR}/include)
  if(WIN32)
    target_compile_definitions(test_${TEST}_pthread_workqueue
                               PRIVATE
                                 _CRT_SECURE_NO_WARNINGS)
  endif()
  target_link_libraries(test_${TEST}_pthread_workqueue
                        PRIVATE
                          pthread_workqueue)

  add_test(NAME ${TEST}
           COMMAND test_${TEST}_pthread_workqueue)
endfunction()

add_unit_test(api
              SOURCES
                api/test.c)
add_unit_test(latency
              SOURCES
                latency/latency.c
                latency/latency.h)
if(NOT WIN32)
  add_unit_test(witem
                DISABLED_TEST
                SOURCES
                  witem_cache/test.c)
endif()

