#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pthread_workqueue.h"

static int work_cnt;

pthread_workqueue_t wq;

void additem(void *( *func)(void *), 
             void * arg)
{
    int rv;
    
    rv = pthread_workqueue_additem_np(wq, func, arg, NULL, NULL);
    if (rv != 0)
        errx(1, "unable to add item: %s", strerror(rv));
}

void *
compute(void *arg)
{
    int i,j;

    /* Do some computation */
    for (i = 0, j = time(NULL); i < 5000000; i++) {
       j += (i % 10);
    }
    printf("j=%d\n", j);
    return (NULL);
}


void *
sleepy(void *msg)
{
    printf("%s\n", (char *) msg);
    if (strcmp(msg, "done") == 0)
        exit(0);
    sleep(random() % 6);
    return (NULL);
}

void *
lazy(void *arg)
{
    sleep(3);
    printf("item %lu complete\n", (unsigned long) arg);
	work_cnt--;
    return (NULL);
}

void
run_blocking_test(void)
{
	const int rounds = 50;
	work_cnt = rounds;
    for (unsigned long i = 0; i < rounds; i++) {
        additem(lazy, (void *) i);
    }
	while (work_cnt > 0)
		sleep(1);
}

void
run_cond_wait_test(void)
{
	const int rounds = 10;

	sleep(3);	/* Allow time for the workers to enter pthread_cond_wait() */
	work_cnt = rounds;
    for (unsigned long i = 0; i < rounds; i++) {
        additem(lazy, (void *) i);
		sleep(1);
    }
	while (work_cnt > 0)
		sleep(1);
}

void
run_load_test(void)
{
    char buf[16];
    for (int i = 0; i < 1024; i++) {
        sprintf(buf, "%d", i);
        additem(sleepy, strdup(buf));
        additem(compute, NULL);
    }
    additem(sleepy, "done");
}

int main() {
    int rv;

    pthread_workqueue_init_np();
    rv = pthread_workqueue_create_np(&wq, NULL);
    if (rv != 0)
        errx(1, "unable to add item: %s", strerror(rv));

    //run_deadlock_test();
    run_cond_wait_test();
    run_blocking_test();
    //run_load_test();

	puts("All tests completed.\n");
    exit(0);
}
