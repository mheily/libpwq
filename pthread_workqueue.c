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
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <unistd.h>

#include "pthread_workqueue.h"

#ifdef PTHREAD_WORKQUEUE_DEBUG
# define dbg_puts(str)           fprintf(stderr, "%s(): %s\n", __func__,str)
# define dbg_printf(fmt,...)     fprintf(stderr, "%s(): "fmt"\n", __func__,__VA_ARGS__)
# define dbg_perror(str)         fprintf(stderr, "%s(): %s: %s\n", __func__,str, strerror(errno))
#else
# define dbg_puts(str)           ;
# define dbg_printf(fmt,...)     ;
# define dbg_perror(str)         ;
#endif 

#define WORKQUEUE_MAX    512            /* DEADWOOD */
#define WORKER_MAX       512            /* Maximum # of worker threads */

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
    unsigned int         id;          /* Index within the workers struct */
    pthread_t            tid;
    bool                 busy;
    int                  overcommit;
    unsigned int         flags;
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

static struct worker     workers[WORKER_MAX];
static unsigned int      worker_max, 
                         worker_min, 
                         worker_cnt;

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
    struct worker *self = (struct worker *) arg;
    struct work *witem;

    for (;;) {
        pthread_mutex_lock(&wqlist_mtx);
        witem = wqlist_scan();
        if (witem == NULL)
            do {
                dbg_printf("worker %u waiting for work", self->id);
                pthread_cond_wait(&wqlist_has_work, &wqlist_mtx);
                witem = wqlist_scan();
            } while (witem == NULL);

        /* Invoke the callback function */
        self->busy = true;
        witem->func(witem->func_arg);
        self->busy = false;
        free(witem);

        dbg_printf("worker %u finished work", self->id);
    }
    /* NOTREACHED */
    return (NULL);
}

static void
worker_start(struct worker *wkr, int flags) 
{
    wkr->busy = false;
    wkr->id = wkr - &workers[0];
    wkr->flags = flags;
    pthread_create(&wkr->tid, NULL, worker_main, wkr);
    dbg_printf("created a new worker (id=%d)", wkr->id);
}

static void
wq_init(void)
{
    worker_cnt = 0;
    worker_min = sysconf(_SC_NPROCESSORS_ONLN) * 2;
    worker_max = sysconf(_SC_NPROCESSORS_ONLN) * 10;

    /* Create the minimum number of threads */
    for (worker_cnt = 0; worker_cnt < worker_min; worker_cnt++) 
        worker_start(&workers[worker_cnt], 0);

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

/*
 * Public API
 */

int
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
pthread_main_np(void)
{
    int i;
    sigset_t sigmask;

    if (!initialized)
        return (-1);

    /* Block all signals */
    sigfillset (&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);

    for (;;) {

        /* Check if there are any idle workers */
        for (i = 0; i < worker_cnt; i++) {
            if (! workers[i].busy) 
                goto not_busy;
        }

        /* Start a new thread if all workers are busy */
        if (worker_cnt < worker_max)
            worker_start(&workers[worker_cnt++], 0);

not_busy:
        sleep(1);
    }

    /*NOTREACHED*/
    return (-1);
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
        workq->queueprio = 1;
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
