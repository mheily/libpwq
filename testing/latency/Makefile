CC=gcc

latency:
	$(CC) -m64 -I../../include -L../.. -R../.. -o latency latency.c -lpthread -lrt -lpthread_workqueue

check: latency
	./latency

clean:
	rm -f latency
