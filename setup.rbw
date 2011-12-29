#
# Copyright (c) 2011 Mark Heily <mark@heily.com>
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

$VERBOSE = true
$LOAD_PATH << 'makeconf'

require 'makeconf'

# FIXME: use arrays instead of strings for cflags and ldflags
sources = [ 'src/*.c' ]
cflags = [ '-I./include', '-I./src' ]
ldadd = []
ldflags = []
if Platform.is_windows?
  sources.push 'src/windows/*.c'
# For GCC:
#  cflags += ' -mthreads'
#  ldadd.push '-mthreads'
else
  cflags.push '-Wall -Wextra -Werror -D_XOPEN_SOURCE=600 -D__EXTENSIONS__ -D_GNU_SOURCE -std=c99'
  sources.push 'src/posix/*.c'
  ldadd.push '-lpthread', '-lrt'
end
if Platform.is_solaris?
  ldflags.push ' -lumem'
end

Makeconf.configure Project.new(
  :id => 'libpthread_workqueue',
  :version => '0.8.2',
  :license => 'BSD',
  :author => 'Mark Heily',
  :summary => 'pthread_workqueue library',
  :description => 'pthread_workqueue library',
  :extra_dist => ['LICENSE', 'src/*.[ch]', 'src/*/*.[ch]'],
  :manpages => 'pthread_workqueue.3',
  :headers => 'pthread_workqueue.h',
  :libraries => {
     'libpthread_workqueue' => {
        :cflags => cflags,
        :sources => sources,
        :ldadd => ldadd,
        },
  },
  :tests => {
    'api' => {
        :sources => [ 'testing/api/test.c' ],
        :ldadd => ['-lpthread_workqueue', ldadd ]
        },
    'latency' => {
        :sources => [ 'testing/latency/latency.c' ],
        :ldadd => ['-lpthread_workqueue', ldadd ]
    },
    'witem_cache' => {
        :sources => [ 'testing/witem_cache/test.c' ],
        :ldadd => ['-lpthread_workqueue', ldadd ]
    },
  }
)

#pre_configure_hook() {
#  if [ "$debug" = "yes" ] ; then
#      cflags="$cflags -g3 -O0 -DPTHREAD_WORKQUEUE_DEBUG -rdynamic"
#  else
#      cflags="$cflags -g -O2"
#  fi
#}
