CC=gcc

include ../../config.mk

latency:
	$(CC) $(CFLAGS) -I../../include -L../.. -o latency latency.c -lpthread -lrt -lpthread_workqueue

macosx:
	$(CC) $(CFLAGS) -I../../include -L../.. -R../.. -o latencym latency.c -lpthread

check: latency
	LD_LIBRARY_PATH=../..:/usr/sfw/lib/amd64 ./latency

clean:
	rm -f latency
	rm -f latencym
