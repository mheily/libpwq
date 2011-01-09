dispatch_api:
	clang -o dispatch_api dispatch_api.c -lpthread -lrt -ldispatch -lBlocksRuntime -lkqueue -lpthread_workqueue -lm

check: dispatch_api
	./dispatch_api

clean:
	rm -f dispatch_api
