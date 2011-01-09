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

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/queue.h>
#include <unistd.h>
#ifdef __sun
#include <sys/loadavg.h>
#endif

#include "pthread_workqueue.h"

/* Function prototypes */
static unsigned int get_load_average(void);
static void * worker_main(void *arg);
static unsigned int get_process_limit(void);

#ifdef PTHREAD_WORKQUEUE_DEBUG
# define dbg_puts(str)           fprintf(stderr, "%s(): %s\n", __func__,str)
# define dbg_printf(fmt,...)     fprintf(stderr, "%s(): "fmt"\n", __func__,__VA_ARGS__)
# define dbg_perror(str)         fprintf(stderr, "%s(): %s: %s\n", __func__,str, strerror(errno))
#else
# define dbg_puts(str)           ;
# define dbg_printf(fmt,...)     ;
# define dbg_perror(str)         ;
#endif 

/* GCC atomic builtins. 
 * See: http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html 
 */
#ifdef __sun
# include <atomic.h>
# define atomic_inc      atomic_inc_32
# define atomic_dec      atomic_dec_32
#else
# define atomic_inc(p)   __sync_add_and_fetch((p), 1)
# define atomic_dec(p)   __sync_sub_and_fetch((p), 1)
#endif

/* The total number of priority levels. */
#define WORKQ_NUM_PRIOQUEUE 3

/* Signatures/magic numbers.  */
#define PTHREAD_WORKQUEUE_SIG       0xBEBEBEBE
#define PTHREAD_WORKQUEUE_ATTR_SIG  0xBEBEBEBE 

struct work {
    STAILQ_ENTRY(work)   item_entry; 
    void               (*func)(void *);
    void                *func_arg;
    unsigned int         flags;
    unsigned int         gencount;
};

struct worker {
    LIST_ENTRY(worker)   entries;
    pthread_t            tid;
    enum {
        WORKER_STATE_SLEEPING,
        WORKER_STATE_RUNNING,
        WORKER_STATE_ZOMBIE,
    } state;
};

struct _pthread_workqueue {
    unsigned int         sig;    /* Unique signature for this structure */
    unsigned int         flags;
    int                  queueprio;
    int                  overcommit;
    LIST_ENTRY(_pthread_workqueue) wqlist_entry;
    STAILQ_HEAD(,work)   item_listhead;
    pthread_spinlock_t   mtx;
};

static unsigned int      cpu_count;

static LIST_HEAD(, worker) workers;
static unsigned int      worker_min;

static LIST_HEAD(, _pthread_workqueue) wqlist[WORKQ_NUM_PRIOQUEUE];
static pthread_mutex_t   wqlist_mtx;
static pthread_cond_t    wqlist_has_work;
static int               wqlist_has_manager;
static unsigned int      wqlist_work_counter;
static pthread_cond_t    manager_init = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t   manager_mtx = PTHREAD_MUTEX_INITIALIZER;

/*
static pthread_once_t    init_once_control = 
#if defined(__SUNPRO_C) || (defined(__sun) && defined(__clang__))
                            { PTHREAD_ONCE_INIT };
#else
                              PTHREAD_ONCE_INIT;
#endif
*/

static pthread_attr_t    detached_attr;

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
        witem = wqlist_scan();
        if (witem == NULL)
            do {
                self->state = WORKER_STATE_SLEEPING;
                pthread_cond_wait(&wqlist_has_work, &wqlist_mtx);
                witem = wqlist_scan();
            } while (witem == NULL);

        if (witem->func == NULL) {
            dbg_puts("worker exiting..");
            self->state = WORKER_STATE_ZOMBIE;
            free(witem);
            pthread_exit(0);
        }

        /* Invoke the callback function */
        self->state = WORKER_STATE_RUNNING;
        atomic_dec(&wqlist_work_counter);
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
        pthread_mutex_unlock(&wqlist_mtx);
        pthread_cond_signal(&wqlist_has_work);

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
    struct worker *wkr;
    unsigned int load_max = cpu_count * 2;
    unsigned int worker_max;
    int i;
    sigset_t sigmask, oldmask;
    struct {
        unsigned int load,
                     count,
                     sleeping,
                     running;
    } st;
    
    worker_max = get_process_limit();
    st.load = get_load_average();

    /* Block all signals */
    sigfillset(&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, &oldmask);

    /* Create the minimum number of workers */
    st.count = 0;
    for (i = 0; i < worker_min; i++) {
        worker_start();
        st.count++;
    }

    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);

    pthread_cond_signal(&manager_init);

    for (;;) {

        dbg_printf("load=%u work_count=%u workers=%u max_workers=%u",
                   st.load, wqlist_work_counter, st.count, worker_max
                );

        /* Examine each worker and update statistics */
        st.sleeping = 0;
        st.running = 0;
        LIST_FOREACH(wkr, &workers, entries) {
            switch (wkr->state) {
                case WORKER_STATE_SLEEPING:
                    st.sleeping++;
                    break;

                case WORKER_STATE_RUNNING:
                    st.running++;
                    /* TODO: check for stalled worker */
                    break;

                case WORKER_STATE_ZOMBIE:
                    LIST_REMOVE(wkr, entries);
                    st.count--;
                    free(wkr);
                    break;
            }
        }

        if (st.sleeping == 0) {
            st.load = get_load_average();
            if (st.load < load_max) {
                dbg_puts("All workers are busy, spawning another worker");
                if (worker_start() == 0)
                    st.count++;
            }
        }

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

        sleep(1);
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

static int
valid_workq(pthread_workqueue_t workq) 
{
    if (workq->sig == PTHREAD_WORKQUEUE_SIG)
        return (1);
    else
        return (0);
}


static unsigned int
get_process_limit(void)
{
    struct rlimit rlim;

    if (getrlimit(RLIMIT_NPROC, &rlim) < 0) {
        dbg_perror("getrlimit(2)");
        return (50);
    } else {
        return (rlim.rlim_max);
    }
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

/*
 * Public API
 */

int __attribute__ ((constructor))
pthread_workqueue_init_np(void)
{
    LIST_INIT(&workers);
    pthread_mutex_init(&wqlist_mtx, NULL);

    cpu_count = (unsigned int) sysconf(_SC_NPROCESSORS_ONLN);

    pthread_attr_init(&detached_attr);
    pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);

    /* Determine the initial thread pool constraints */
    worker_min = cpu_count;

    manager_start();

    pthread_cond_wait(&manager_init, &manager_mtx);
    pthread_mutex_unlock(&manager_mtx);

    dbg_puts("pthread_workqueue library initialized");
    return (0);
}

int
pthread_workqueue_create_np(pthread_workqueue_t *workqp,
                            const pthread_workqueue_attr_t * attr)
{
    pthread_workqueue_t workq;

    if ((attr != NULL) && ((attr->sig != PTHREAD_WORKQUEUE_ATTR_SIG) ||
         (attr->queueprio < 0) || (attr->queueprio > WORKQ_NUM_PRIOQUEUE)))
        return (EINVAL);
    if ((workq = calloc(1, sizeof(*workq))) == NULL)
        return (ENOMEM);
    workq->sig = PTHREAD_WORKQUEUE_SIG;
    workq->flags = 0;
    STAILQ_INIT(&workq->item_listhead);
    pthread_spin_init(&workq->mtx, PTHREAD_PROCESS_PRIVATE);
    if (attr == NULL) {
        workq->queueprio = WORKQ_DEFAULT_PRIOQUEUE;
        workq->overcommit = 0;
    } else {
        workq->queueprio = attr->queueprio;
        workq->overcommit = attr->overcommit;
    }

    pthread_mutex_lock(&wqlist_mtx);
    LIST_INSERT_HEAD(&wqlist[workq->queueprio], workq, wqlist_entry);
    pthread_mutex_unlock(&wqlist_mtx);

    dbg_printf("created queue %p", workq);

    *workqp = workq;
    return (0);
}

int
pthread_workqueue_additem_np(pthread_workqueue_t workq,
                     void (*workitem_func)(void *), void * workitem_arg,
                     pthread_workitem_handle_t * itemhandlep, 
                     unsigned int *gencountp)
{
    struct work *witem;
    
    dbg_puts("entering additem...");

    if (valid_workq(workq) == 0)
        return (EINVAL);

    /* TODO: Keep a free list to avoid frequent malloc/free() penalty */
    witem = malloc(sizeof(*witem));
    if (witem == NULL)
        return (ENOMEM);
    witem->gencount = 0;
    witem->func = workitem_func;
    witem->func_arg = workitem_arg;
    witem->flags = 0;
    witem->item_entry.stqe_next = 0;

    /* TODO: possibly use a separate mutex or some kind of atomic CAS */
    pthread_mutex_lock(&wqlist_mtx);
    if (!wqlist_has_manager)
        manager_start();
    pthread_mutex_unlock(&wqlist_mtx);

    pthread_spin_lock(&workq->mtx);
    STAILQ_INSERT_TAIL(&workq->item_listhead, witem, item_entry);
    pthread_spin_unlock(&workq->mtx);
    atomic_inc(&wqlist_work_counter);
    pthread_cond_signal(&wqlist_has_work);

    if (itemhandlep != NULL)
        *itemhandlep = (pthread_workitem_handle_t *) witem;
    if (gencountp != NULL)
        *gencountp = witem->gencount;

    dbg_printf("added an item to queue %p", workq);

    return (0);
}

int
pthread_workqueue_attr_init_np(pthread_workqueue_attr_t *attr)
{
    attr->queueprio = WORKQ_DEFAULT_PRIOQUEUE;
    attr->sig = PTHREAD_WORKQUEUE_ATTR_SIG;
    attr->overcommit = 0;
    return (0);
}

int
pthread_workqueue_attr_destroy_np(pthread_workqueue_attr_t *attr)
{
    if (attr->sig == PTHREAD_WORKQUEUE_ATTR_SIG)
        return (0);
    else
        return (EINVAL); /* Not an attribute struct. */
}

int
pthread_workqueue_attr_getovercommit_np(
        const pthread_workqueue_attr_t *attr, int *ocommp)
{
    if (attr->sig == PTHREAD_WORKQUEUE_ATTR_SIG) {
        *ocommp = attr->overcommit;
        return (0);
    } else 
        return (EINVAL); /* Not an attribute struct. */
}

int
pthread_workqueue_attr_setovercommit_np(pthread_workqueue_attr_t *attr,
                           int ocomm)
{
    if (attr->sig == PTHREAD_WORKQUEUE_ATTR_SIG) {
        attr->overcommit = ocomm;
        return (0);
    } else
        return (EINVAL);
}

int
pthread_workqueue_attr_getqueuepriority_np(
        pthread_workqueue_attr_t *attr, int *qpriop)
{
    if (attr->sig == PTHREAD_WORKQUEUE_ATTR_SIG) {
        *qpriop = attr->queueprio;
        return (0);
    } else 
        return (EINVAL);
}

int 
pthread_workqueue_attr_setqueuepriority_np(
        pthread_workqueue_attr_t *attr, int qprio)
{
    if (attr->sig == PTHREAD_WORKQUEUE_ATTR_SIG) {
        switch(qprio) {
            case WORKQ_HIGH_PRIOQUEUE:
            case WORKQ_DEFAULT_PRIOQUEUE:
            case WORKQ_LOW_PRIOQUEUE:
                attr->queueprio = qprio;
                return (0);
            default:
                return (EINVAL);
        }
    } else
        return (EINVAL);
}

/*
 * Does not exist in the Apple implementation, but needed on Linux
 * due to a kernel bug that causes the process to become a zombie when
 * the main thread calls pthread_exit().
 *
 * More info:
 *   http://www.0x61.com/forum/viewtopic.php?f=109&t=997736&view=next
 */
void
pthread_workqueue_main_np(void)
{

    //TESTING - dispatch testsuite requires this..
    pthread_exit(0);

    /* 
    struct worker w;
    pthread_mutex_lock(&wqlist_mtx);
    if (wqlist_has_manager) {
        pthread_mutex_unlock(&wqlist_mtx);
        dbg_puts("running as a worker");
        memset(&w, 0, sizeof(w));
        worker_main(&w);
    } else {
        wqlist_has_manager = 1;
        pthread_mutex_unlock(&wqlist_mtx);

        dbg_puts("running as a manager");
        manager_main(NULL);
    }
    */
}
