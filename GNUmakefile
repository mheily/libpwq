# AUTOMATICALLY GENERATED -- DO NOT EDIT
BINDIR = $(EPREFIX)/bin
CC = cc
DATADIR = $(DATAROOTDIR)
DATAROOTDIR = $(PREFIX)/share
DOCDIR = $(DATAROOTDIR)/doc/$(PACKAGE)
EPREFIX = $(PREFIX)
INCLUDEDIR = $(PREFIX)/include
INFODIR = $(DATAROOTDIR)/info
INSTALL ?= /usr/bin/install
LD = cc
LIBDIR = $(EPREFIX)/lib
LIBEXECDIR = $(EPREFIX)/libexec
LOCALEDIR = $(DATAROOTDIR)/locale
LOCALSTATEDIR = $(PREFIX)/var
MANDIR = $(DATAROOTDIR)/man
OLDINCLUDEDIR = /usr/include
PKGDATADIR = $(DATADIR)/$(PACKAGE)
PKGINCLUDEDIR = $(INCLUDEDIR)/$(PACKAGE)
PKGLIBDIR = $(LIBDIR)/$(PACKAGE)
PREFIX = /usr/local
SBINDIR = $(EPREFIX)/sbin
SHAREDSTATEDIR = $(PREFIX)/com
SYSCONFDIR = $(PREFIX)/etc


#
# Detect the canonical system type of the system we are building on
# (build) and the system the package runs on (host)
# 
BUILD_CPU=$(shell uname -m)
HOST_CPU=$(BUILD_CPU)
BUILD_VENDOR=unknown
HOST_VENDOR=$(BUILD_VENDOR)
BUILD_KERNEL=$(shell uname | tr '[A-Z]' '[a-z]')
HOST_KERNEL=$(BUILD_KERNEL)
BUILD_SYSTEM=gnu
HOST_SYSTEM=$(BUILD_SYSTEM)
BUILD_TYPE=$(BUILD_CPU)-$(BUILD_VENDOR)-$(BUILD_KERNEL)-$(BUILD_SYSTEM)
HOST_TYPE=$(HOST_CPU)-$(HOST_VENDOR)-$(HOST_KERNEL)-$(HOST_SYSTEM)

# Allow variables to be overridden via a ./configure script that outputs config.mk
# FIXME -- requires GNU Make
-include config.mk

default: all

all:  libpthread_workqueue.so libpthread_workqueue.a api latency witem_cache

api: testing/api/test.o
	$(LD)  -o api -L . -Wl,-rpath,. -L . $(LDFLAGS) testing/api/test.o libpthread_workqueue.a -lpthread -lrt $(LDADD)

check:  api latency witem_cache
	./api
	./latency
	./witem_cache

clean: 
	rm -f *.rpm
	rm -f libpthread_workqueue-0.8.3.tar.gz
	rm -f src/witem_cache.o
	rm -f src/api.o
	rm -f src/posix/thread_info.o
	rm -f src/posix/manager.o
	rm -f src/posix/thread_rt.o
	rm -f src/linux/load.o
	rm -f src/linux/thread_info.o
	rm -f src/linux/thread_rt.o
	rm -f libpthread_workqueue.so
	rm -f libpthread_workqueue.a
	rm -f testing/api/test.o
	rm -f api
	rm -f testing/latency/latency.o
	rm -f latency
	rm -f testing/witem_cache/test.o
	rm -f witem_cache

config.h: 
	@echo "checking build system type... $(BUILD_TYPE)"
	@echo "checking host system type... $(HOST_TYPE)"
	@echo "/* AUTOMATICALLY GENERATED -- DO NOT EDIT */" > config.h.tmp
	@date > config.log
	@rm -f conftest.c conftest.o
	@echo "creating config.h"
	@mv config.h.tmp config.h

dist: libpthread_workqueue-0.8.3.tar.gz

distclean: clean 
	rm -f GNUmakefile
	rm -f libpthread_workqueue-0.8.3.tar.gz
	rm -f config.h
	rm -f config.yaml
	rm -f rpm.spec

distdir: 
	umask 22 ; mkdir -p '$(distdir)/src'
	umask 22 ; mkdir -p '$(distdir)/src/posix'
	umask 22 ; mkdir -p '$(distdir)/src/linux'
	umask 22 ; mkdir -p '$(distdir)/include'
	umask 22 ; mkdir -p '$(distdir)/src/linux/..'
	umask 22 ; mkdir -p '$(distdir)/src/linux/../posix'
	umask 22 ; mkdir -p '$(distdir)/src/linux/../linux'
	umask 22 ; mkdir -p '$(distdir)/testing/api'
	umask 22 ; mkdir -p '$(distdir)/testing/api/../..'
	umask 22 ; mkdir -p '$(distdir)/testing/api/../../src'
	umask 22 ; mkdir -p '$(distdir)/testing/api/../../src/posix'
	umask 22 ; mkdir -p '$(distdir)/testing/api/../../src/linux'
	umask 22 ; mkdir -p '$(distdir)/testing/latency'
	umask 22 ; mkdir -p '$(distdir)/testing/witem_cache'
	umask 22 ; mkdir -p '$(distdir)/testing/witem_cache/../..'
	umask 22 ; mkdir -p '$(distdir)/testing/witem_cache/../../src'
	umask 22 ; mkdir -p '$(distdir)/testing/witem_cache/../../src/posix'
	umask 22 ; mkdir -p '$(distdir)/testing/witem_cache/../../src/linux'
	cp -RL src/witem_cache.c src/private.h src/debug.h src/api.c src/thread_info.h src/thread_rt.h src/*.c $(distdir)/src
	cp -RL src/posix/platform.h src/posix/thread_info.c src/posix/manager.c src/posix/thread_rt.c src/posix/*.c $(distdir)/src/posix
	cp -RL src/linux/platform.h src/linux/load.c src/linux/thread_info.c src/linux/thread_rt.c src/linux/*.c $(distdir)/src/linux
	cp -RL include/pthread_workqueue.h $(distdir)/include
	cp -RL GNUmakefile configure configure.rb $(distdir)
	cp -RL src/linux/../private.h src/linux/../debug.h $(distdir)/src/linux/..
	cp -RL src/linux/../posix/platform.h $(distdir)/src/linux/../posix
	cp -RL src/linux/../linux/platform.h $(distdir)/src/linux/../linux
	cp -RL testing/api/test.c $(distdir)/testing/api
	cp -RL testing/api/../../config.h $(distdir)/testing/api/../..
	cp -RL testing/api/../../src/private.h testing/api/../../src/debug.h $(distdir)/testing/api/../../src
	cp -RL testing/api/../../src/posix/platform.h $(distdir)/testing/api/../../src/posix
	cp -RL testing/api/../../src/linux/platform.h $(distdir)/testing/api/../../src/linux
	cp -RL testing/latency/latency.c testing/latency/latency.h $(distdir)/testing/latency
	cp -RL testing/witem_cache/test.c $(distdir)/testing/witem_cache
	cp -RL testing/witem_cache/../../config.h $(distdir)/testing/witem_cache/../..
	cp -RL testing/witem_cache/../../src/private.h testing/witem_cache/../../src/debug.h $(distdir)/testing/witem_cache/../../src
	cp -RL testing/witem_cache/../../src/posix/platform.h $(distdir)/testing/witem_cache/../../src/posix
	cp -RL testing/witem_cache/../../src/linux/platform.h $(distdir)/testing/witem_cache/../../src/linux

install: 
	/usr/bin/test -e $(DESTDIR)$(LIBDIR) || $(INSTALL) -d -m 755 $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0644 libpthread_workqueue.so $(DESTDIR)$(LIBDIR)/libpthread_workqueue.so.0.0

latency: testing/latency/latency.o
	$(LD)  -o latency -L . -Wl,-rpath,. -L . $(LDFLAGS) testing/latency/latency.o libpthread_workqueue.a -lpthread -lrt $(LDADD)

libpthread_workqueue-0.8.3.tar.gz: 
	rm -rf libpthread_workqueue-0.8.3
	mkdir libpthread_workqueue-0.8.3
	$(MAKE) distdir distdir=libpthread_workqueue-0.8.3
	rm -rf libpthread_workqueue-0.8.3.tar libpthread_workqueue-0.8.3.tar.gz
	tar cf libpthread_workqueue-0.8.3.tar libpthread_workqueue-0.8.3
	gzip libpthread_workqueue-0.8.3.tar
	rm -rf libpthread_workqueue-0.8.3

libpthread_workqueue.a: src/witem_cache.o src/api.o src/posix/thread_info.o src/posix/manager.o src/posix/thread_rt.o src/linux/load.o src/linux/thread_info.o src/linux/thread_rt.o
ifneq ($(DISABLE_STATIC),1)
	ar cru libpthread_workqueue.a src/witem_cache.o src/api.o src/posix/thread_info.o src/posix/manager.o src/posix/thread_rt.o src/linux/load.o src/linux/thread_info.o src/linux/thread_rt.o
	ranlib libpthread_workqueue.a
endif

libpthread_workqueue.so: src/witem_cache.o src/api.o src/posix/thread_info.o src/posix/manager.o src/posix/thread_rt.o src/linux/load.o src/linux/thread_info.o src/linux/thread_rt.o
	$(LD)  -o libpthread_workqueue.so -shared -fPIC -L . $(LDFLAGS) src/witem_cache.o src/api.o src/posix/thread_info.o src/posix/manager.o src/posix/thread_rt.o src/linux/load.o src/linux/thread_info.o src/linux/thread_rt.o -lpthread -lrt $(LDADD)

package: clean libpthread_workqueue-0.8.3.tar.gz
	rm -rf rpm *.rpm
	mkdir -p rpm/BUILD rpm/RPMS rpm/SOURCES rpm/SPECS rpm/SRPMS
	mkdir -p rpm/RPMS/i386 rpm/RPMS/x86_64
	cp libpthread_workqueue-0.8.3.tar.gz rpm/SOURCES
	rpmbuild --define "_topdir `pwd`/rpm" -bs rpm.spec
	cp rpm.spec rpm/SPECS/rpm.spec
	rpmbuild --define "_topdir `pwd`/rpm" -bb ./rpm/SPECS/rpm.spec
	mv ./rpm/SRPMS/* ./rpm/RPMS/*/*.rpm .
	rm -rf rpm

src/api.o: src/api.c src/private.h src/posix/platform.h src/linux/platform.h include/pthread_workqueue.h src/debug.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -o src/api.o -fPIC -DPIC $(CFLAGS) -c src/api.c

src/linux/load.o: src/linux/load.c src/linux/../private.h src/linux/../posix/platform.h src/linux/../linux/platform.h include/pthread_workqueue.h src/linux/../debug.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -o src/linux/load.o -fPIC -DPIC $(CFLAGS) -c src/linux/load.c

src/linux/thread_info.o: src/linux/thread_info.c src/linux/platform.h src/private.h src/posix/platform.h include/pthread_workqueue.h src/debug.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -o src/linux/thread_info.o -fPIC -DPIC $(CFLAGS) -c src/linux/thread_info.c

src/linux/thread_rt.o: src/linux/thread_rt.c src/linux/platform.h src/private.h src/posix/platform.h include/pthread_workqueue.h src/debug.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -o src/linux/thread_rt.o -fPIC -DPIC $(CFLAGS) -c src/linux/thread_rt.c

src/posix/manager.o: src/posix/manager.c src/posix/platform.h src/private.h src/linux/platform.h include/pthread_workqueue.h src/debug.h src/thread_info.h src/thread_rt.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -o src/posix/manager.o -fPIC -DPIC $(CFLAGS) -c src/posix/manager.c

src/posix/thread_info.o: src/posix/thread_info.c GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -o src/posix/thread_info.o -fPIC -DPIC $(CFLAGS) -c src/posix/thread_info.c

src/posix/thread_rt.o: src/posix/thread_rt.c GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -o src/posix/thread_rt.o -fPIC -DPIC $(CFLAGS) -c src/posix/thread_rt.c

src/witem_cache.o: src/witem_cache.c src/private.h src/posix/platform.h src/linux/platform.h include/pthread_workqueue.h src/debug.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -I./src -Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99 -o src/witem_cache.o -fPIC -DPIC $(CFLAGS) -c src/witem_cache.c

testing/api/test.o: testing/api/test.c testing/api/../../config.h testing/api/../../src/private.h testing/api/../../src/posix/platform.h testing/api/../../src/linux/platform.h include/pthread_workqueue.h testing/api/../../src/debug.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -g -O0 -o testing/api/test.o $(CFLAGS) -c testing/api/test.c

testing/latency/latency.o: testing/latency/latency.c testing/latency/latency.h include/pthread_workqueue.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -g -O0 -o testing/latency/latency.o $(CFLAGS) -c testing/latency/latency.c

testing/witem_cache/test.o: testing/witem_cache/test.c testing/witem_cache/../../config.h testing/witem_cache/../../src/private.h testing/witem_cache/../../src/posix/platform.h testing/witem_cache/../../src/linux/platform.h include/pthread_workqueue.h testing/witem_cache/../../src/debug.h GNUmakefile
	$(CC) -DHAVE_CONFIG_H -I. -I./include -g -O0 -o testing/witem_cache/test.o $(CFLAGS) -c testing/witem_cache/test.c

uninstall: 
	rm -f $(DESTDIR)$(LIBDIR)/libpthread_workqueue.so

witem_cache: testing/witem_cache/test.o
	$(LD)  -o witem_cache -L . -Wl,-rpath,. -L . $(LDFLAGS) testing/witem_cache/test.o libpthread_workqueue.a -lpthread -lrt $(LDADD)
