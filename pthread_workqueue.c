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
#include <sys/queue.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef __sun
#include <sys/loadavg.h>
#endif

#include "pthread_workqueue.h"

/* Function prototypes */
static void    *worker_main(void *arg);

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

/* Signatures/magic numbers.  */
#define PTHREAD_WORKQUEUE_SIG       0xBEBEBEBE
#define PTHREAD_WORKQUEUE_ATTR_SIG  0xBEBEBEBE 

/* The number of seconds that a worker will wait for work before terminating */
#define WORKER_IDLE_TIMEOUT         3

struct work {
    STAILQ_ENTRY(work)   item_entry; 
    void               (*func)(void *);
    void                *func_arg;
    unsigned int         flags;
    unsigned int         gencount;
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
static pthread_mutex_t   workers_mtx;
static unsigned int      workers_count;

/* A list of workqueues, sorted by priority */
static LIST_HEAD(, _pthread_workqueue) wqlist;
static pthread_mutex_t   wqlist_mtx;
static pthread_cond_t    wqlist_has_work;
static int               initialized = 0;

/* The caller must hold the wqlist_mtx. */
static struct work *
wqlist_scan(void)
{
    pthread_workqueue_t workq;
    struct work *witem;

    /* FIXME: Will not scale to large numbers of wqlists */
    LIST_FOREACH(workq, &wqlist, wqlist_entry) {
        if (STAILQ_EMPTY(&workq->item_listhead))
            continue;
        pthread_mutex_unlock(&wqlist_mtx);

        pthread_spin_lock(&workq->mtx);
        witem = STAILQ_FIRST(&workq->item_listhead);
        if (witem != NULL)
            STAILQ_REMOVE_HEAD(&workq->item_listhead, item_entry);
        pthread_spin_unlock(&workq->mtx);
        return (witem);
    }

    return (NULL);
}

static void *
worker_main(void *arg)
{
    struct work *witem;
    struct timeval tp;
    struct timespec ts;
    int rv;

    dbg_puts("worker starting up");

    for (;;) {

        /* Determine the absolute timeout value */
        gettimeofday(&tp, NULL);
        ts.tv_sec  = tp.tv_sec;
        ts.tv_nsec = tp.tv_usec * 1000;
        ts.tv_sec += WORKER_IDLE_TIMEOUT; 

        /* Acquire a work item */
        pthread_mutex_lock(&wqlist_mtx);
        witem = wqlist_scan();
        while (witem == NULL) {
            rv = pthread_cond_timedwait(&wqlist_has_work, &wqlist_mtx, &ts);
            if (rv == ETIMEDOUT) {
                pthread_mutex_unlock(&wqlist_mtx);
                atomic_dec(&workers_count);
                dbg_puts("worker exiting due to inactivity");
                pthread_exit(0);
            } else if (rv < 0) {
                abort(); /* FIXME: too extreme */
            }

            witem = wqlist_scan();
        }

        /* Invoke the callback function */
        witem->func(witem->func_arg);
        free(witem);
    }
    /* NOTREACHED */
    return (NULL);
}

static void
wq_init(void)
{
    LIST_INIT(&workers);
    pthread_mutex_init(&workers_mtx, NULL);
    pthread_mutex_init(&wqlist_mtx, NULL);

    /* TODO: Handle platforms that support hot-plugging of CPUs.  */
    cpu_count = (unsigned int) sysconf(_SC_NPROCESSORS_ONLN);

    workers_count = 0;

    initialized = 1;
}

static int
valid_workq(pthread_workqueue_t workq) 
{
    if (workq->sig == PTHREAD_WORKQUEUE_SIG)
        return (1);
    else
        return (0);
}

#if DEADWOOD
static int
avg_runqueue_length(void)
{
    double loadavg;
    int retval;

    /* TODO: proper error handling */
    if (getloadavg(&loadavg, 1) != 1) {
        dbg_perror("getloadavg(3)");
        return (1);
    }
    if (loadavg > INT_MAX || loadavg < 0)
        loadavg = 1;

    retval = (unsigned int) loadavg / cpu_count;
    dbg_printf("load_avg=%e / cpu_count=%u := %d avg_runqueue_len\n",
            loadavg,
            cpu_count,
            retval);

    return (retval);
}
#endif

/*
 * Public API
 */

int __attribute__ ((constructor))
pthread_workqueue_init_np(void) 
{
#if defined(__SUNPRO_C)
    static pthread_once_t once_control = { PTHREAD_ONCE_INIT };
#else
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
#endif

    pthread_once(&once_control, wq_init);
    return (0);
}

int
pthread_workqueue_create_np(pthread_workqueue_t *workqp,
                            const pthread_workqueue_attr_t * attr)
{
    pthread_workqueue_t workq;
    pthread_workqueue_t cur;

    if ((attr != NULL) && (attr->sig != PTHREAD_WORKQUEUE_ATTR_SIG))
        return (EINVAL);
    if ((workq = calloc(1, sizeof(*workq))) == NULL)
        return (ENOMEM);
    workq->sig = PTHREAD_WORKQUEUE_SIG;
    workq->flags = 0;
    STAILQ_INIT(&workq->item_listhead);
    if (attr == NULL) {
        workq->queueprio = WORKQ_DEFAULT_PRIOQUEUE;
        workq->overcommit = 0;
    } else {
        workq->queueprio = attr->queueprio;
        workq->overcommit = attr->overcommit;
    }
    pthread_spin_init(&workq->mtx, PTHREAD_PROCESS_PRIVATE);

    pthread_mutex_lock(&wqlist_mtx);
    LIST_FOREACH(cur, &wqlist, wqlist_entry) {
        if (cur->queueprio <= workq->queueprio) 
            break;
    }
    if (cur == NULL)
        LIST_INSERT_HEAD(&wqlist, workq, wqlist_entry);
    else
        LIST_INSERT_AFTER(cur, workq, wqlist_entry);
    pthread_mutex_unlock(&wqlist_mtx);

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
    pthread_t tid;
    
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

    pthread_spin_lock(&workq->mtx);
#if TODO
    if ((workq->flags & 
           (PTHREAD_WORKQ_IN_TERMINATE | PTHREAD_WORKQ_DESTROYED)) != 0) {
            pthread_spin_unlock(&workq->mtx);
           free(witem);
           *itemhandlep = 0;
           return (ESRCH);
       }
#endif //TODO
    STAILQ_INSERT_TAIL(&workq->item_listhead, witem, item_entry);
    pthread_spin_unlock(&workq->mtx);
    pthread_cond_signal(&wqlist_has_work);

    if (itemhandlep != NULL)
        *itemhandlep = (pthread_workitem_handle_t *) witem;
    if (gencountp != NULL)
        *gencountp = witem->gencount;

    //dbg_printf("added an item to queue %p", workq);

    /* Make sure there are enough workers. */
    /* NOTE: There is a race condition when accessing workers_count. 
       The consequences of losing the race is that an extra thread is created,
       which will self-terminate if there is not enough work to do.
       The performance benefit of not using a global mutex makes
       this an acceptable risk.

       XXX-FIXME the other race is not so nice, when all workers die off
       before the workers_count is decremented.
       */
    if (workers_count < cpu_count) {
        if (pthread_create(&tid, NULL, worker_main, NULL) != 0) {
            dbg_perror("pthread_create(3)");
            return (-1);
        }
        atomic_inc(&workers_count);
    }

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
