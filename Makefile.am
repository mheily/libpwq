# FIXME: only supports Linux for now
ACLOCAL_AMFLAGS = -I m4

if INSTALL
    lib_LTLIBRARIES = libpthread_workqueue.la
    include_HEADERS = include/pthread_workqueue.h
    dist_man_MANS = pthread_workqueue.3
else
    noinst_LTLIBRARIES = libpthread_workqueue.la
endif

libpthread_workqueue_la_SOURCES = src/api.c src/witem_cache.c src/posix/manager.c  src/posix/thread_info.c src/posix/thread_rt.c  \
  ./include/pthread_workqueue.h \
  ./src/posix/platform.h \
  ./src/thread_info.h \
  ./src/private.h \
  ./src/thread_rt.h \
  ./src/debug.h

if LINUX
    libpthread_workqueue_la_SOURCES += src/linux/load.c  src/linux/thread_info.c src/linux/thread_rt.c src/linux/platform.h
endif

if !BIONIC_LIBC
libpthread_workqueue_la_LIBADD = -lpthread -lrt
endif

libpthread_workqueue_la_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -fvisibility=hidden

check_PROGRAMS = test_api test_latency test_witem_cache
TESTS = test_api test_latency test_witem_cache

test_api_SOURCES = testing/api/test.c ./testing/api/posix_semaphore.h
test_api_CFLAGS = -I$(top_srcdir)/include -I$(builddir)
if BIONIC_LIBC
test_api_LDADD = libpthread_workqueue.la
else
test_api_LDADD = libpthread_workqueue.la -lpthread -lrt
endif

test_latency_SOURCES = testing/latency/latency.c ./testing/latency/latency.h
test_latency_CFLAGS = -I$(top_srcdir)/include
if BIONIC_LIBC
test_latency_LDADD = libpthread_workqueue.la
else
test_latency_LDADD = libpthread_workqueue.la -lpthread -lrt
endif

test_witem_cache_SOURCES = testing/witem_cache/test.c
test_witem_cache_CFLAGS = -I$(top_srcdir)/include
if BIONIC_LIBC
test_witem_cache_LDADD = libpthread_workqueue.la
else
test_witem_cache_LDADD = libpthread_workqueue.la -lpthread -lrt
endif