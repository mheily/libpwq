/*-
 * Copyright (c) 2010, Mark Heily <mark@heily.com>
 * Copyright (c) 2009, Stacey Son <sson@freebsd.org>
 * Copyright (c) 2000-2008, Apple Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "platform.h"
#include "private.h"
#include "pthread_workqueue.h"

/* Function prototypes */
static unsigned int get_load_average(void);
static void * worker_main(void *arg);
static unsigned int get_process_limit(void);
static void manager_start(void);

static unsigned int      cpu_count;

static LIST_HEAD(, worker) workers;
static unsigned int      worker_min;

static LIST_HEAD(, _pthread_workqueue) wqlist[WORKQ_NUM_PRIOQUEUE];
static pthread_mutex_t   wqlist_mtx;

static pthread_cond_t    wqlist_has_work;
static int               wqlist_has_manager;
static pthread_cond_t    manager_init_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t   manager_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_attr_t    detached_attr;

static struct {
    unsigned int    load,
                    count,
                    idle;
    unsigned int    sb_wake_pending;
    pthread_mutex_t sb_wake_mtx;
    pthread_cond_t  sb_wake_cond;
} scoreboard;

int
manager_init(void)
{
    int i;

    LIST_INIT(&workers);
    pthread_mutex_init(&wqlist_mtx, NULL);
    for (i = 0; i < WORKQ_NUM_PRIOQUEUE; i++)
        LIST_INIT(&wqlist[i]);

    cpu_count = (unsigned int) sysconf(_SC_NPROCESSORS_ONLN);

    pthread_attr_init(&detached_attr);
    pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);

    /* Initialize the scoreboard */
    pthread_cond_init(&scoreboard.sb_wake_cond, NULL);
    pthread_mutex_init(&scoreboard.sb_wake_mtx, NULL);

    /* Determine the initial thread pool constraints */
    worker_min = cpu_count;

    manager_start();

    pthread_cond_wait(&manager_init_cond, &manager_mtx);
    pthread_mutex_unlock(&manager_mtx);
    return (0);
}

void
manager_workqueue_create(struct _pthread_workqueue *workq)
{
    pthread_mutex_lock(&wqlist_mtx);
    LIST_INSERT_HEAD(&wqlist[workq->queueprio], workq, wqlist_entry);
    pthread_mutex_unlock(&wqlist_mtx);
}

/* The caller must hold the wqlist_mtx. */
static struct work *
wqlist_scan(void)
{
    pthread_workqueue_t workq;
    struct work *witem = NULL;
    int i;

    for (i = 0; i < WORKQ_NUM_PRIOQUEUE; i++) {
        LIST_FOREACH(workq, &wqlist[i], wqlist_entry) {
            if (STAILQ_EMPTY(&workq->item_listhead))
            continue;

            witem = STAILQ_FIRST(&workq->item_listhead);
            if (witem != NULL)
                STAILQ_REMOVE_HEAD(&workq->item_listhead, item_entry);
            pthread_mutex_unlock(&wqlist_mtx);
            goto out;
        }
    }

out:
    return (witem);
}

static void *
worker_main(void *arg)
{
    struct worker *self = (struct worker *) arg;
    struct work *witem;

    for (;;) {
        pthread_mutex_lock(&wqlist_mtx);

        atomic_inc(&scoreboard.idle);
        self->state = WORKER_STATE_SLEEPING;
        
        while ((witem = wqlist_scan()) == NULL)
            pthread_cond_wait(&wqlist_has_work, &wqlist_mtx);

        if (witem->func == NULL) {
            dbg_puts("worker exiting..");
            self->state = WORKER_STATE_ZOMBIE;
            atomic_dec(&scoreboard.idle);
            free(witem);
            pthread_exit(0);
        }

        atomic_dec(&scoreboard.idle);
        self->state = WORKER_STATE_RUNNING;

        /* Force the manager thread to wakeup if all workers are busy */
        dbg_printf("count=%u idle=%u wake_pending=%u", 
            scoreboard.count, scoreboard.idle,  scoreboard.sb_wake_pending);
        if (scoreboard.idle == 0 && !scoreboard.sb_wake_pending) {
            dbg_puts("asking manager to wake up");
            pthread_mutex_lock(&scoreboard.sb_wake_mtx);
            scoreboard.sb_wake_pending = 1;
            pthread_cond_signal(&scoreboard.sb_wake_cond);
            pthread_mutex_unlock(&scoreboard.sb_wake_mtx);
        }

        /* Invoke the callback function */
        witem->func(witem->func_arg);
        free(witem);
    }
    /* NOTREACHED */
    return (NULL);
}

static int
worker_start(void) 
{
    struct worker *wkr;

    wkr = calloc(1, sizeof(*wkr));
    if (wkr == NULL) {
        dbg_perror("calloc(3)");
        return (-1);
    }

    if (pthread_create(&wkr->tid, &detached_attr, worker_main, wkr) != 0) {
        dbg_perror("pthread_create(3)");
        return (-1);
    }

    LIST_INSERT_HEAD(&workers, wkr, entries);

    dbg_puts("created a new thread");

    return (0);
}

//static int
int
worker_stop(void) 
{
    struct work *witem;
    pthread_workqueue_t workq;
    int i;

    witem = calloc(1, sizeof(*witem));
    if (witem == NULL)
        return (-1);

    pthread_mutex_lock(&wqlist_mtx);
    for (i = 0; i < WORKQ_NUM_PRIOQUEUE; i++) {
        workq = LIST_FIRST(&wqlist[i]);
        if (workq == NULL)
            continue;

        STAILQ_INSERT_TAIL(&workq->item_listhead, witem, item_entry);
        pthread_cond_signal(&wqlist_has_work);
        pthread_mutex_unlock(&wqlist_mtx);

        return (0);
    }

    /* FIXME: this means there are no workqueues.. should never happen */
    dbg_puts("Attempting to add a workitem without a workqueue");
    abort();

    return (-1);
}

static void *
manager_main(void *unused)
{
    //struct worker *wkr;
    unsigned int load_max = cpu_count * 2;
    unsigned int worker_max;
    int i;
    sigset_t sigmask, oldmask;
    
    worker_max = get_process_limit();
    scoreboard.load = get_load_average();

    /* Block all signals */
    sigfillset(&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask);

    /* Create the minimum number of workers */
    scoreboard.count = 0;
    for (i = 0; i < worker_min; i++) {
        worker_start();
        scoreboard.count++;
    }

    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);

    pthread_cond_signal(&manager_init_cond);

    for (;;) {

        dbg_printf("load=%u idle=%u workers=%u max_workers=%u",
                   scoreboard.load, scoreboard.idle, scoreboard.count, worker_max
                );

#if DEADWOOD
        /* Examine each worker and update statistics */
        scoreboard.sleeping = 0;
        scoreboard.running = 0;
        LIST_FOREACH(wkr, &workers, entries) {
            switch (wkr->state) {
                case WORKER_STATE_SLEEPING:
                    scoreboard.sleeping++;
                    break;

                case WORKER_STATE_RUNNING:
                    scoreboard.running++;
                    /* TODO: check for stalled worker */
                    break;

                case WORKER_STATE_ZOMBIE:
                    LIST_REMOVE(wkr, entries);
                    scoreboard.count--;
                    free(wkr);
                    break;
            }
        }
#endif

        dbg_puts("manager is sleeping");
        pthread_mutex_lock(&scoreboard.sb_wake_mtx);
        pthread_cond_wait(&scoreboard.sb_wake_cond, &scoreboard.sb_wake_mtx);
        
        dbg_puts("manager is awake");

        if (scoreboard.idle == 0) {
            scoreboard.load = get_load_average();
            if (scoreboard.load < load_max) {
                dbg_puts("All workers are busy, spawning another worker");
                if (worker_start() == 0)
                    scoreboard.count++;
            }
        }
        scoreboard.sb_wake_pending = 0;
        pthread_mutex_unlock(&scoreboard.sb_wake_mtx);

#if DEADWOOD
        /* Check if there is too much work and not enough workers */
        if ((wqlist_work_counter > st.count) && (st.count < worker_max)) {
            if (st.load < cpu_count) {
                dbg_puts("Too much work, spawning another worker");
                worker_start();
            } else {
                dbg_puts("System load is too high to spawn another worker.");
            }
        }

        /* Check if there are too many active workers and not enough CPUs */
        if (choke_timeout == 0) {
            if ((st.load > cpu_count) && (st.count > worker_max)) {
                    dbg_puts("Workload is too high, removing one thread from the thread pool");
                    worker_stop();
            } else if ((len < 3) && (wqlist_work_counter > 0)) {
                dbg_puts("Some workers may be stalled, will add another thread");
                worker_start();
            }
            choke_timeout = max_choke;
        } else {
            choke_timeout--;
        }

        /* Check if there are too many workers and not enough work*/
        if (wqlist_work_counter == 0) {
            if (st.count > worker_min) {
                dbg_puts("Removing one thread from the thread pool");
                worker_stop();
                idle_timeout++;
            }

            if (idle_timeout > 0) {
                idle_timeout--;
            } else if (st.count > 0) {
                dbg_puts("Removing one thread from the thread pool");
                worker_stop();
                idle_timeout++;
            } else if (st.count == 0) {

                /* Confirm that all workers have exited */
                if (! LIST_EMPTY(&workers))
                    idle_timeout++;

                if (idle_timeout == 0) {
                    dbg_puts("killing the manager thread");
                    pthread_mutex_lock(&wqlist_mtx);
                    wqlist_has_manager = 0;
                    pthread_mutex_unlock(&wqlist_mtx);
                    pthread_exit(0);
                } else {
                    dbg_puts("waiting for all workers to exit");
                }
            }
        }
#endif

    }

    /*NOTREACHED*/
    return (NULL);
}

static void
manager_start(void)
{
    pthread_t tid;
    int rv;

    dbg_puts("starting the manager thread");

    do {
        rv = pthread_create(&tid, &detached_attr, manager_main, NULL);
        if (rv == EAGAIN) {
            sleep(1);
        } else if (rv != 0) {
            /* FIXME: not nice */
            dbg_printf("thread creation failed, rv=%d", rv);
            abort();
        }
    } while (rv != 0);

    wqlist_has_manager = 1;
}

void
manager_workqueue_additem(struct _pthread_workqueue *workq, struct work *witem)
{
    /* TODO: possibly use a separate mutex or some kind of atomic CAS */
    pthread_mutex_lock(&wqlist_mtx);

    if (!wqlist_has_manager)
        manager_start();

    pthread_spin_lock(&workq->mtx);
    STAILQ_INSERT_TAIL(&workq->item_listhead, witem, item_entry);
    pthread_spin_unlock(&workq->mtx);

    pthread_cond_signal(&wqlist_has_work);
    pthread_mutex_unlock(&wqlist_mtx);
}


static unsigned int
get_process_limit(void)
{
#if __linux__
    struct rlimit rlim;

    if (getrlimit(RLIMIT_NPROC, &rlim) < 0) {
        dbg_perror("getrlimit(2)");
        return (50);
    } else {
        return (rlim.rlim_max);
    }
#else
    /* Solaris doesn't define this limit anywhere I can see.. */
    return (128);
#endif
}

static unsigned int
get_load_average(void)
{
    double loadavg;

    /* TODO: proper error handling */
    if (getloadavg(&loadavg, 1) != 1) {
        dbg_perror("getloadavg(3)");
        return (1);
    }
    if (loadavg > INT_MAX || loadavg < 0)
        loadavg = 1;

    return ((int) loadavg);
}
