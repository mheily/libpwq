program="libpthread_workqueue"
version="0.1"
abi_major="0"
abi_minor="0"
abi_version="$abi_major.$abi_minor"
cflags="-fpic -Wall -Werror -D_XOPEN_SOURCE=600 -D_GNU_SOURCE -std=c99 -m64"
ldflags=""
sources="pthread_workqueue.c"
libdepends=""
deps=""
mans="pthread_workqueue.3"
headers="pthread_workqueue.h"
extra_dist="LICENSE"
subdirs=""

pre_configure_hook() {
  if [ "$debug" = "yes" ] ; then
      cflags="$cflags -g3 -O0 -DPTHREAD_WORKQUEUE_DEBUG -rdynamic"
  else
      cflags="$cflags -g -O2"
  fi
}

post_configure_hook() {
    return
}
