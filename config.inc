program="libpthread_workqueue"
version="0.2"
abi_major="0"
abi_minor="0"
abi_version="$abi_major.$abi_minor"
cflags="-Wall -Werror -D_XOPEN_SOURCE=600 -D_GNU_SOURCE -std=c99 -I./include -I./src"
ldflags=""
sources="src/api.c"
libdepends=""
deps="src/*.h"
mans="pthread_workqueue.3"
headers="include/pthread_workqueue.h"
extra_dist="LICENSE"
subdirs=""

# Package metadata
pkg_summary="pthread_workqueue library"
pkg_description="pthread_workqueue library"
license="BSD"
author="Mark Heily"

pre_configure_hook() {
  if [ "$debug" = "yes" ] ; then
      cflags="$cflags -g3 -O0 -DPTHREAD_WORKQUEUE_DEBUG -rdynamic"
  else
      cflags="$cflags -g -O2"
  fi
  check_header err.h
}

post_configure_hook() {
    sources="$sources src/posix/manager.c"
    case "$target" in
    windows)
    	;;
    solaris)
    	ldflags="$ldflags -m64"
    	cflags="$cflags -m64"
	    ;;
    solaris|linux)
        cflags="$cflags -fpic"
    	ldflags="$ldflags -shared"
	;;
    esac
}
