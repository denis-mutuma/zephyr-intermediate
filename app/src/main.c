#include "zephyr/toolchain.h"
#include <stdatomic.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_INF);

#define QUEUE_SIZE 8
#define PRODUCER_PRIORITY 5
#define CONSUMER_PRIORITY 4
#define HEALTH_PRIORITY 6
#define PRODUCER_PERIOD 50
#define CONSUMER_PERIOD 50
#define STACK_SIZE 1024
#define WARN_LEVEL ((QUEUE_SIZE * 3) / 4)

// for consumer
#define WDT_TIMEOUT_MS 2000
#define STUCK_AFTER 20

K_MSGQ_DEFINE(pipeline, sizeof(uint32_t), QUEUE_SIZE, 4);

static void consumer_wdt_callback(int channel_id, void *user_data) {
    LOG_ERR("[WDT] channel %d fired, thread=%s, used=%u/%u",
    channel_id,
    k_thread_name_get((k_tid_t)user_data),
    k_msgq_num_used_get(&pipeline),
    QUEUE_SIZE);
}

static void producer_fn(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    uint32_t seq = 0;
    
    while(1) {
        int err = k_msgq_put(&pipeline, &seq, K_NO_WAIT);
        if(err == 0) {
            LOG_INF("[PROD] seq=%u used %u/%u",
                        seq, k_msgq_num_used_get(&pipeline), QUEUE_SIZE);
        } else {
            LOG_WRN("[PROD] queue full, dropping seq=%u used %u/%u",
                        seq, k_msgq_num_used_get(&pipeline), QUEUE_SIZE);   
        }
        seq++;
        k_msleep(50);
    }
}

static void consumer_fn(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    uint32_t seq;
    uint32_t count = 0;
    int wdt_id;

    k_thread_name_set(k_current_get(), "consumer");
    wdt_id = task_wdt_add(WDT_TIMEOUT_MS, consumer_wdt_callback, (void *)k_current_get());

    if(wdt_id < 0) {
        LOG_ERR("[CONS] task_wdt_add failed: $d", wdt_id);
        return;
    }

    while(1) {
        if(k_msgq_get(&pipeline, &seq, K_FOREVER) == 0) {
            LOG_INF("[CONS] seq=%u", seq);
            task_wdt_feed(wdt_id);
            count++;

            if(count == STUCK_AFTER) {
                LOG_WRN("[CONS] simulating stall with long k_sleep");
                k_sleep(K_SECONDS(8));
            }
        }
        k_msleep(50);
    }
}

static void health_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while(1) {
        uint32_t used = k_msgq_num_used_get(&pipeline);

        if(used >= WARN_LEVEL) {
            LOG_WRN("[HEALTH] queue at %u/%u (75%% threshold)", 
                used, QUEUE_SIZE);
        } else {
            LOG_INF("[HEALTH] queue at %u/%u", used, QUEUE_SIZE);
        }
        k_msleep(100);
    }
}

K_THREAD_DEFINE(producer_thread, STACK_SIZE, producer_fn, NULL, NULL, NULL, 
    PRODUCER_PRIORITY, 0, 0);

K_THREAD_DEFINE(consumer_thread, STACK_SIZE, consumer_fn, NULL, NULL, NULL, 
    CONSUMER_PRIORITY, 0, 0);

K_THREAD_DEFINE(health_thread, STACK_SIZE, health_fn, NULL, NULL, NULL, 
    HEALTH_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("=== L5 Demo 1: Reliability under pressure ===");
    
    int ret = task_wdt_init(NULL);
    if(ret != 0) {
        LOG_ERR("task_wdt_init_failed: %d", ret);
        return 0;
    }

    return 0;
}