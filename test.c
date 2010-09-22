#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pthread_workqueue.h"

static int work_cnt;

pthread_workqueue_t wq;

extern int pthread_main_np(void);

static void *
wq_manager(void *arg)
{
    pthread_main_np();
    return (NULL);
}

void additem(void (*func)(void *), 
             void * arg)
{
    int rv;
    
    rv = pthread_workqueue_additem_np(wq, *func, arg, NULL, NULL);
    if (rv != 0)
        errx(1, "unable to add item: %s", strerror(rv));
}

void
compute(void *arg)
{
    static const int nval = 10000;
    int val[nval];
    int i,j;

    /* Do some useless computation */
    for (i = 0; i < nval; i++) {
        val[i] = INT_MAX;
    }
    for (j = 0; j < 50000; j++) {
        for (i = 0; i < nval; i++) {
            val[i] /= 3;
            val[i] *= 2;
            val[i] /= 4;
            val[i] *= 5;
        }
    }
    printf("compute task finished\n");
}


void
sleepy(void *msg)
{
    printf("%s\n", (char *) msg);
    if (strcmp(msg, "done") == 0)
        exit(0);
    sleep(random() % 6);
}

void
lazy(void *arg)
{
    sleep(3);
    printf("item %lu complete\n", (unsigned long) arg);
	work_cnt--;
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

/* Try to overwhelm the CPU with computation requests */
void
run_stress_test(void)
{
	const int rounds = 1000;
	work_cnt = rounds;
    for (int i = 0; i < rounds; i++) {
        additem(compute, NULL);
    }
	while (work_cnt > 0)
		sleep(1);
}


int main() {
    int rv;
    pthread_t tid;

    pthread_workqueue_init_np();
    rv = pthread_workqueue_create_np(&wq, NULL);
    if (rv != 0)
        errx(1, "unable to add item: %s", strerror(rv));

    while (pthread_create(&tid, NULL, wq_manager, NULL) != 0) {
        sleep(2);
    }

    run_stress_test();
    exit(0);//XXX-FIXME TEMP

    //run_deadlock_test();
    run_cond_wait_test();
    run_blocking_test();
    //run_load_test();

    puts("Sleeping for 2 minutes.. the number of threads should drop\n");
    sleep(120);

	puts("All tests completed.\n");
    exit(0);
}
