#include <string.h>

#include "timers.h"
#include "utils.h"

void dill_timers_init(struct dill_timers *timers, int64_t now, timer_fired_callback cb) {
    timers->timer_fired = cb;
    timers->current_now = now;
    timers->nearest_deadline = now;
    memset(timers->buckets, 0, sizeof(timers->buckets));
    fprintf(stderr, "Initializing timers %lld\n", now);
}

unsigned dill_timers_sleep_for(struct dill_timers *timers, int64_t now,
                               unsigned max_sleep) {
    int64_t delta = 0;
    if(timers->nearest_deadline > now) {
        delta = timers->nearest_deadline - now;
        if(delta > max_sleep) delta = max_sleep;
        fprintf(stderr, "Calculated delta %d\n", (int)delta);
    } else {
        while(
            !timers->buckets[timers->nearest_deadline % DILL_TIMERS_NBUCKETS]) {
            timers->nearest_deadline++;
            delta = timers->nearest_deadline - now;
            if(delta >= max_sleep) {
                delta = max_sleep;
                break;
            }
        }
        fprintf(stderr, "Computed delta %d\n", (int)delta);
    }
    return delta;
}

unsigned dill_timers_expire(struct dill_timers *timers, int64_t now) {
    unsigned n_fired = 0;

    for(; timers->current_now <= now; timers->current_now++) {
        struct dill_timer_handle **bucket =
            &timers->buckets[timers->current_now % DILL_TIMERS_NBUCKETS];
        fprintf(stderr, "Current now %lld; now %lld bucket %p\n", timers->current_now, now, *bucket);
        struct dill_timer_handle *timer = *bucket;
        if(dill_slow(timer)) { /* Not all buckets are filled with timers. */
            struct dill_timer_handle *nt = NULL;
            for(; timer != nt; timer = nt) {
                nt = dill_cont(dill_list_next(&timer->list),
                               struct dill_timer_handle, list);
                if(timer->deadline <= now) {
                    n_fired++;
                    fprintf(stderr, "Timer about to expire with deadline %lld\n", timer->deadline);
                    /* Remove the timer and fire it up. */
                    if(dill_list_empty(&timer->list)) {
                        fprintf(stderr, "  this was the last timer in the bucket\n");
                        /* This is the last timer in the bucket */
                        *bucket = 0;
                        break;
                    } else {
                        fprintf(stderr, "  more timers in the bucket\n");
                        if(*bucket == timer) {
                            *bucket = nt;
                        }
                        dill_list_erase(&timer->list);
                        timers->timer_fired(timers, timer);
                    }
                }
            }
        }
    }

    if(timers->nearest_deadline < timers->current_now)
        timers->nearest_deadline = timers->current_now;

    fprintf(stderr, "Timer next nearest deadline %lld, now %lld, n_fired=%d\n",
        timers->nearest_deadline, now, n_fired);
    return n_fired;
}

void dill_timer_schedule(struct dill_timers *timers,
                         struct dill_timer_handle *timer, int64_t deadline) {
    if(timers->nearest_deadline > deadline)
        timers->nearest_deadline = deadline;
    struct dill_timer_handle **bucket =
        &timers->buckets[deadline % DILL_TIMERS_NBUCKETS];
    *(int64_t*)&timer->deadline = deadline;    /* const_casting. */
    fprintf(stderr, "Adding timer for %lld deadline\n", deadline);
    if(dill_slow(*bucket)) {
        dill_list_insert(&timer->list, &(*bucket)->list);
    } else {
        dill_list_init(&timer->list);
        *bucket = timer;
    }
}
void dill_timer_unschedule(struct dill_timers *timers,
                           struct dill_timer_handle *timer) {
    struct dill_timer_handle **bucket =
        &timers->buckets[timer->deadline % DILL_TIMERS_NBUCKETS];

    if(dill_list_empty(&timer->list)) {
        dill_assert(*bucket == timer);
        *bucket = NULL;
    } else {
        if(*bucket == timer) {
            /* Replace bucket with a different timer which is still alive. */
            *bucket = dill_cont(dill_list_next(&timer->list),
                                struct dill_timer_handle, list);
        }
        dill_list_erase(&timer->list);
    }
}
