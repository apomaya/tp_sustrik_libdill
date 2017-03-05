#ifndef DILL_TIMERS_INCLUDED
#define DILL_TIMERS_INCLUDED

#include "list.h"

struct dill_timers;   /* Forward declaration */
struct dill_timer_handle;   /* Forward declaration */

typedef void (*timer_fired_callback)(struct dill_timers *,
                                     struct dill_timer_handle *);

/*
 * All timers in a single wheel.
 * Since our resolution is 1ms, the timer wheel can have
 * amortized O(1) operation on about 10k timers spread evenly,
 * deteriorating into O(d) where D is number of active timers
 * in the system, divided by 10k.
 */
#define DILL_TIMERS_NBUCKETS    10000
struct dill_timers {
    timer_fired_callback timer_fired;
    int64_t current_now;
    int64_t nearest_deadline;
    struct dill_timer_handle *buckets[DILL_TIMERS_NBUCKETS];
};

/*
 * A single timer (intrusive data structure).
 */
struct dill_timer_handle {
    const int64_t deadline;
    struct dill_list list; /* Position in the timer wheel bucket. */
};

void dill_timers_init(struct dill_timers *, int64_t now_start,
                      timer_fired_callback);

/*
 * Get back with the delta (in now() units) for which we have
 * to sleep in order to reach the nearest timer.
 * The delta can be 0 but will not exceed max_sleep.
 * 0 means that 1 or more timers are ready to fire now.
 */
unsigned dill_timers_sleep_for(struct dill_timers *, int64_t now,
                               unsigned max_sleep);

/*
 * Returns the number of timers expired in this cycle.
 */
unsigned dill_timers_expire(struct dill_timers *, int64_t now);

void dill_timer_schedule(struct dill_timers *, struct dill_timer_handle *,
                         int64_t deadline);
void dill_timer_unschedule(struct dill_timers *, struct dill_timer_handle *);

#endif
