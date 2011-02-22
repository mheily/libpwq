/*
 * Copyright (c) 2011 Joakim Johansson <jocke@tbricks.com>.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "pthread_workqueue.h"

// Run settings
#define SECONDS_TO_RUN 10
#define WORKQUEUE_COUNT 6
#define GENERATOR_THREAD_COUNT 4
#define SLEEP_BEFORE_START 0
#define FORCE_BUSY_LOOP 0

// Data rates
// #define EVENTS_GENERATED_PER_TICK 25   // simulate some small bursting 
// #define EVENT_GENERATION_FREQUENCY 1000  // events/s base rate, need to use busy loop = 1 if > 100Hz due to nanosleep resolution 

#define EVENTS_GENERATED_PER_TICK 250   
#define EVENT_GENERATION_FREQUENCY 100

#define AGGREGATE_DATA_RATE_PER_SECOND (EVENT_GENERATION_FREQUENCY * EVENTS_GENERATED_PER_TICK)
#define EVENTS_TO_GENERATE (SECONDS_TO_RUN * AGGREGATE_DATA_RATE_PER_SECOND)
#define TOTAL_DATA_PER_SECOND (AGGREGATE_DATA_RATE_PER_SECOND*GENERATOR_THREAD_COUNT)

#define NANOSECONDS_PER_SECOND 1000000000
#define DISTRIBUTION_BUCKETS 20 // 1us per bucket
#define EVENT_TIME_SLICE (NANOSECONDS_PER_SECOND / EVENT_GENERATION_FREQUENCY)
#define SYSTEM_CLOCK_RESOLUTION 100

typedef unsigned long mytime_t;

struct wq_event 
{
	unsigned int queue_index; 
	mytime_t start_time;
};

struct wq_statistics 
{
	unsigned int min; 
	unsigned int max; 
	double avg; 
	unsigned int total; 
	unsigned int count; 
	unsigned int count_over_threshold; 
    unsigned int distribution[DISTRIBUTION_BUCKETS];
};

struct wq_statistics workqueue_statistics[WORKQUEUE_COUNT]; 
pthread_workqueue_t workqueues[WORKQUEUE_COUNT]; 
pthread_t generator_threads[GENERATOR_THREAD_COUNT];

struct wq_statistics global_statistics;
unsigned int global_stats_used = 0;

#define PERCENTILE_COUNT 8 
double percentiles[PERCENTILE_COUNT] = {50.0, 80.0, 98.0, 99.0, 99.5, 99.8, 99.9, 99.99};

static unsigned long gettime(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        fprintf(stderr, "Failed to get monotonic clock! errno = %d\n", errno);   
    return ((ts.tv_sec * NANOSECONDS_PER_SECOND) + ts.tv_nsec);
}

// real resolution on solaris is at best system clock tick, i.e. 100Hz unless having the
// high res system clock (1000Hz in that case)

static void my_sleep(unsigned long nanoseconds)
{
	struct timespec timeout0;
	struct timespec timeout1;
	struct timespec* tmp;
	struct timespec* t0 = &timeout0;
	struct timespec* t1 = &timeout1;
	
	t0->tv_sec = nanoseconds / NANOSECONDS_PER_SECOND;
	t0->tv_nsec = nanoseconds % NANOSECONDS_PER_SECOND;

	while ((nanosleep(t0, t1) == (-1)) && (errno == EINTR))
	{
		tmp = t0;
		t0 = t1;
		t1 = tmp;
	}
    
    return;
}

static void _process_data(void* context)
{
	struct wq_event *event = (struct wq_event *) context;
	mytime_t elapsed_time;
    
	elapsed_time = gettime() - event->start_time;
    
	workqueue_statistics[event->queue_index].avg = ((workqueue_statistics[event->queue_index].count * workqueue_statistics[event->queue_index].avg) + elapsed_time) / (workqueue_statistics[event->queue_index].count + 1);
	workqueue_statistics[event->queue_index].total += elapsed_time;
	workqueue_statistics[event->queue_index].count += 1;
    
    if (elapsed_time < workqueue_statistics[event->queue_index].min || 
        workqueue_statistics[event->queue_index].min == 0) 
        workqueue_statistics[event->queue_index].min = elapsed_time;
	
    if (elapsed_time > workqueue_statistics[event->queue_index].max)
        workqueue_statistics[event->queue_index].max = elapsed_time;
    
    if ((elapsed_time / 1000) < DISTRIBUTION_BUCKETS)
        workqueue_statistics[event->queue_index].distribution[(int)(elapsed_time / 1000)] += 1;
    else  
        workqueue_statistics[event->queue_index].distribution[DISTRIBUTION_BUCKETS-1] += 1;

    // We zero these out to detect buffer overruns
    event->start_time = 0;
    event->queue_index = 0;
    
    return;
}

static void _gather_statistics(void* context)
{
	unsigned long queue_index = (unsigned long) context;
    unsigned long i;
    
    if (workqueue_statistics[queue_index].count > 0)
    {
        global_stats_used ++;
        
        global_statistics.avg = ((global_statistics.count * global_statistics.avg) + (workqueue_statistics[queue_index].avg * workqueue_statistics[queue_index].count)) / (global_statistics.count + workqueue_statistics[queue_index].count);
        global_statistics.total += workqueue_statistics[queue_index].total;
        global_statistics.count += workqueue_statistics[queue_index].count;
        
        if (workqueue_statistics[queue_index].min < global_statistics.min || global_statistics.min == 0) 
            global_statistics.min = workqueue_statistics[queue_index].min;
        
        if (workqueue_statistics[queue_index].max > global_statistics.max)
            global_statistics.max = workqueue_statistics[queue_index].max;
        
        for (i = 0; i < DISTRIBUTION_BUCKETS; i++)
            global_statistics.distribution[i] += workqueue_statistics[queue_index].distribution[i];
    }

	return;
}

static void *_generate_simulated_events(void *t)
{
	long i, current_event_index = 0, current_events_generated = 0, overhead;
    mytime_t start, current, overhead_start = 0, overhead_end = 0;
	struct wq_event *current_event;
	struct wq_event *wq_events; // really used as a ring buffer without safety here, we should only do tests where consumer keep up...
	
	wq_events = malloc(sizeof(struct wq_event) * AGGREGATE_DATA_RATE_PER_SECOND);
	memset(wq_events, 0, (sizeof(struct wq_event) * AGGREGATE_DATA_RATE_PER_SECOND));
    
	while (current_events_generated < EVENTS_TO_GENERATE)
	{
        start = current = gettime();
        overhead = overhead_end - overhead_start;
        
        // wait until we have waited proper amount of time for current rate
        // we should remove overhead of previous lap to not lag behind in data rate
        // one call to gethrtime() alone is around 211ns on Nehalem 2.93
        // use busy waiting in case the frequency is higher than the supported resolution of nanosleep()
        
        if ((EVENT_GENERATION_FREQUENCY > SYSTEM_CLOCK_RESOLUTION) || FORCE_BUSY_LOOP)
        {
            while ((current - start) < (EVENT_TIME_SLICE - overhead))
                current = gettime();            
        }
        else
        {
            my_sleep(EVENT_TIME_SLICE - overhead);
        }
        
        overhead_start = gettime();

		// Perform a small microburst for this tick
		for (i = 0; (i < EVENTS_GENERATED_PER_TICK) && (current_events_generated < EVENTS_TO_GENERATE); i++, current_events_generated++)
		{
			current_event = &wq_events[current_event_index++];
            
            // We use wq_events as a simple ring buffer, just check we are not overrunning
            // the processing here, should not happen during normal load tests unless long time is spent in the processing
            if ((current_event->start_time != 0) || (current_event->queue_index != 0))
            {
                fprintf(stderr, "wq_events overrun, %ld, %ld, %ld, reconfigure test with different parameters.\n", i, current_event->start_time, current_event->queue_index);

                while ((current_event->start_time != 0) || (current_event->queue_index != 0)) // allow potential overrun to clear
                    sleep(1);
            }
            
			current_event->start_time = gettime();
			current_event->queue_index = (current_event->start_time % WORKQUEUE_COUNT);

            if (pthread_workqueue_additem_np(workqueues[current_event->queue_index], _process_data, current_event, NULL, NULL) != 0)
                fprintf(stderr, "pthread_workqueue_additem_np failed\n");
            
			if (current_event_index == AGGREGATE_DATA_RATE_PER_SECOND)
				current_event_index = 0;
		}

        overhead_end = gettime();
	}	

	return (void *) current_events_generated;
}

// joins the generator threads when we are ready to print statistics
static void *_wait_for_all_events(void *t)
{
	unsigned long i, j, total_events = 0, last_percentile = 0, accumulated_percentile = 0;
	void *events_done;
	mytime_t real_start, real_end;
    
	real_start = gettime();
    
	for (i=0; i<GENERATOR_THREAD_COUNT; i++)
	{
		(void) pthread_join(generator_threads[i], &events_done);
		total_events += (unsigned long) events_done;
	}

    real_end = gettime();
    sleep(10);
    my_sleep(EVENT_TIME_SLICE * 2); // allow processing to finish, should use a semaphore really
    
	printf("Collecting statistics...\n");
	
	for (i = 0; i < WORKQUEUE_COUNT; i++)
        if (pthread_workqueue_additem_np(workqueues[i], _gather_statistics, (void *) i, NULL, NULL) != 0)
            fprintf(stderr, "pthread_workqueue_additem_np failed\n");
    
	printf("Test is done, run time was %.3f seconds, %.1fM events generated and processed.\n", (double)((double)(real_end - real_start) / (double) NANOSECONDS_PER_SECOND), total_events/1000000.0); 
	
	printf("Global dispatch queue aggregate statistics for %d queues: %dM events, min = %d ns, avg = %.1f ns, max = %d ns\n",
           global_stats_used, global_statistics.count/1000000, global_statistics.min, global_statistics.avg, global_statistics.max);
    
    printf("\nDistribution:\n");
    for (i = 0; i < DISTRIBUTION_BUCKETS; i++)
    {                   
        printf("%3ld us: %d ", i, global_statistics.distribution[i]);
        for (j=0; j<(((double) global_statistics.distribution[i] / (double) global_statistics.count) * 400.0); j++)
            printf("*");
        printf("\n");
    }
    
    printf("\nPercentiles:\n");
    
    for (i = 0; i < DISTRIBUTION_BUCKETS; i++)
    {        
        while ((last_percentile < PERCENTILE_COUNT) && ((100.0 * ((double) accumulated_percentile / (double) global_statistics.count)) > percentiles[last_percentile]))
        {
            printf("%.2f < %ld us\n", percentiles[last_percentile], i-1);
            last_percentile++;
        }
        accumulated_percentile += global_statistics.distribution[i];        
    }
    
    while ((last_percentile < PERCENTILE_COUNT) && ((100.0 * ((double) accumulated_percentile / (double) global_statistics.count)) > percentiles[last_percentile]))
    {
        printf("%.2f > %d us\n", percentiles[last_percentile], DISTRIBUTION_BUCKETS-1);
        last_percentile++;
    }
    
	exit(0);
	
	return NULL;
}	


int main(int argc, const char * argv[])
{
	int i;
    pthread_workqueue_attr_t attr;
    
	memset(&workqueues, 0, sizeof(workqueues));
	memset(&workqueue_statistics, 0, sizeof(workqueue_statistics));
	memset(&global_statistics, 0, sizeof(global_statistics));
	
	for (i = 0; i < WORKQUEUE_COUNT; i++)
	{
        if (pthread_workqueue_attr_init_np(&attr) != 0)
            fprintf(stderr, "Failed to set workqueue attributes\n");
        
        if (pthread_workqueue_attr_setqueuepriority_np(&attr, (i % (WORKQ_LOW_PRIOQUEUE + 1))) != 0) // spread it round-robin in terms of prio
            fprintf(stderr, "Failed to set workqueue priority\n");
        
        if (pthread_workqueue_create_np(&workqueues[i], &attr) != 0)
            fprintf(stderr, "Failed to create workqueue\n");
	}
    
	if (SLEEP_BEFORE_START > 0)
	{
		printf("Sleeping for %d seconds to allow for processor set configuration.\n",SLEEP_BEFORE_START);
		sleep(SLEEP_BEFORE_START); 		
	}
	
    printf("%d workqueues, running for %d seconds at %d Hz, %d events per tick.\n",WORKQUEUE_COUNT, SECONDS_TO_RUN, EVENT_GENERATION_FREQUENCY, EVENTS_GENERATED_PER_TICK);
    
	printf("Running %d generator threads at %dK events/s, the aggregated data rate is %dK events/s. %.2f MB is used for %.2fM events.\n",
           GENERATOR_THREAD_COUNT,AGGREGATE_DATA_RATE_PER_SECOND/1000, TOTAL_DATA_PER_SECOND/1000,
           GENERATOR_THREAD_COUNT * ((double)(sizeof(struct wq_event) * AGGREGATE_DATA_RATE_PER_SECOND + sizeof(workqueues))/(1024.0*1024.0)), 
           GENERATOR_THREAD_COUNT * EVENTS_TO_GENERATE/1000000.0);
    
	for (i = 0; i < GENERATOR_THREAD_COUNT; i++)
		(void) pthread_create(&generator_threads[i], NULL, _generate_simulated_events, NULL); 

    _wait_for_all_events(NULL);
        
	return 0;
}
