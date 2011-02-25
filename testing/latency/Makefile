CC=gcc

latency:
	$(CC) -m64 -I../../include -L../.. -R../.. -o latency latency.c -lpthread -lrt -lpthread_workqueue

macosx:
	$(CC) -m64 -I../../include -L../.. -R../.. -o latencym latency.c -lpthread

check-linux:
	LD_LIBRARY_PATH=../.. ./latency

check: latency
	./latency

clean:
	rm -f latency
	rm -f latencym
