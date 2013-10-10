# FIXME: only supports Linux for now

lib_LTLIBRARIES = libpthread_workqueue.la
include_HEADERS = include/pthread_workqueue.h
man_MANS = pthread_workqueue.3

libpthread_workqueue_la_SOURCES = src/api.c src/witem_cache.c src/posix/manager.c  src/posix/thread_info.c src/posix/thread_rt.c src/linux/load.c  src/linux/thread_info.c src/linux/thread_rt.c
libpthread_workqueue_la_CFLAGS = -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99

check_PROGRAMS = test_api test_latency test_witem_cache
TESTS = test_api test_latency test_witem_cache

test_api_SOURCES = testing/api/test.c
test_api_CFLAGS = -I./include
test_api_LDADD = libpthread_workqueue.a -lpthread -lrt

test_latency_SOURCES = testing/latency/latency.c
test_latency_CFLAGS = -I./include
test_latency_LDADD = libpthread_workqueue.a -lpthread -lrt

test_witem_cache_SOURCES = testing/witem_cache/test.c
test_witem_cache_CFLAGS = -I./include
test_witem_cache_LDADD = libpthread_workqueue.a -lpthread -lrt
