#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(l1_task1, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define T_LOW 7
#define T_MED 5
#define T_HIGH 3
#define T_COOP (-1)

void t_low_fn(void *p1, void *p2, void *p3) {
    LOG_INF("[T_LOW] runs and then sleeps for 300ms");

    for(int i = 0; i < 5; ++i) {
        LOG_INF("[T_LOW] running - step=%d tick=%u", i+1, k_uptime_get_32());
        k_msleep(300);  
    }
    
    LOG_INF("[T_LOW] done");
}

void t_med_fn(void *p1, void *p2, void *p3) {
    LOG_INF("[T_MED] runs and then sleeps for 200ms");
    
    for(int i = 0; i < 5; ++i) {
        LOG_INF("[T_MED] running - step=%d tick=%u", i+1, k_uptime_get_32());
        k_msleep(200);
    }

    LOG_INF("[T_MED] done");
}

void t_high_fn(void *p1, void *p2, void *p3) {
    LOG_INF("[T_HIGH] runs and then sleeps for 100ms");
    
    for(int i = 0; i < 5; ++i) {
        LOG_INF("[T_HIGH] running - step=%d tick=%u", i+1, k_uptime_get_32());
        k_msleep(100);
    }

    LOG_INF("[T_HIGH] done");
}

/*
 * Observed interleaving of the three preemptible threads. A smaller
 * priority number is more urgent: T_HIGH is 3 and sleeps 100 ms, T_MED
 * is 5 and sleeps 200 ms, T_LOW is 7 and sleeps 300 ms. While several
 * are ready, the most urgent runs. k_msleep() makes that thread unready,
 * so the next most urgent ready thread runs, and a wakeup of a more
 * urgent thread preempts a less urgent one.
 * Observed: main prints its banner at tick 1000, right after T_COOP
 * returns and before any preemptible thread. Those threads then log
 * step 1 as T_HIGH, T_MED, T_LOW. T_HIGH keeps logging about every
 * 100 ms until tick 1550; T_MED and T_LOW log only while every more
 * urgent thread is asleep.
 *
 * Cooperative behavior: T_COOP (priority -1) busy-waits, so the tick
 * cannot take the CPU and the five steps finish before any lower-priority
 * thread runs. k_yield() only lets ready threads of equal or higher
 * priority run, so the lower-priority threads progress only after the
 * cooperative thread returns, sleeps, or blocks. Nothing of equal or
 * higher priority is ready, so the yield returns at once and those
 * threads run only after t_coop_fn returns.
 */

void t_coop_fn(void *p1, void *p2, void *p3) {
    LOG_INF("[T_COOP] runs 5 iteration of busy work and then yields");

    for(int i = 0; i < 5; ++i) {
        LOG_INF("[T_COOP] running - step=%d tick=%u", i+1, k_uptime_get_32());
        k_busy_wait(200000);
    }

    LOG_INF("[T_COOP] yielding");

    k_yield();
    LOG_INF("[T_COOP] done");
}

K_THREAD_DEFINE(t_low, STACK_SIZE, t_low_fn, NULL, NULL, NULL, T_LOW, 0, 0);
K_THREAD_DEFINE(t_med, STACK_SIZE, t_med_fn, NULL, NULL, NULL, T_MED, 0, 0);
K_THREAD_DEFINE(t_high, STACK_SIZE, t_high_fn, NULL, NULL, NULL, T_HIGH, 0, 0);
K_THREAD_DEFINE(t_coop, STACK_SIZE, t_coop_fn, NULL, NULL, NULL, T_COOP, 0, 0);

int main(void) {
    LOG_INF("=== l1 task 1: thread interleaving ===");
    LOG_INF("LOW prio=%d MED prio=%d HIGH prio=%d COOP prio=%d", T_LOW, T_MED, T_HIGH, T_COOP);
    return 0;
}
