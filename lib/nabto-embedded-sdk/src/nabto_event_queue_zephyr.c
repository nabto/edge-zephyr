#include "nabto_event_queue_zephyr.h"

#include <platform/np_timestamp_wrapper.h>
#include <platform/np_allocator.h>

#include <platform/np_logging.h>
#include <platform/interfaces/np_event_queue.h>

#include <zephyr/kernel.h>

#define LOG NABTO_LOG_MODULE_PLATFORM

struct np_event {
    struct k_work work;
    struct k_work_delayable dWork;
    np_event_callback cb;
    bool lockCore;
    void* cbData;
    struct nabto_event_queue_zephyr* queue;
};

static np_error_code create_event(struct np_event_queue* obj, np_event_callback cb, void* cbData, struct np_event** event);
static void destroy_event(struct np_event* event);
static void post_event(struct np_event* event);
static bool post_event_maybe_double(struct np_event* event);

static void cancel_event(struct np_event* event);

static void post_timed_event(struct np_event* event, uint32_t milliseconds);

static void post_event_no_core_lock(struct np_event* event);

static void handle_work_event(struct k_work* item);
static void handle_scheduled_event(struct k_work* item);

static void handle_event(struct np_event* ev);


static struct np_event_queue_functions module = {
    .create = &create_event,
    .destroy = &destroy_event,
    .post = &post_event,
    .post_maybe_double = &post_event_maybe_double,
    .cancel = &cancel_event,
    .post_timed = &post_timed_event,
    .post_no_core_lock = &post_event_no_core_lock,
};

struct np_event_queue nabto_event_queue_zephyr_get_impl(struct nabto_event_queue_zephyr* queue)
{
    struct np_event_queue eq;
    eq.mptr = &module;
    eq.data = queue;
    return eq;
}

np_error_code nabto_event_queue_zephyr_init(struct nabto_event_queue_zephyr* queue, struct nabto_device_mutex* coreMutex)
{
    queue->stopped = false;
    queue->coreMutex = coreMutex;
    return NABTO_EC_OK;
}

// handle work when the zephyr work queue says it's ready to be handled.
void handle_work_event(struct k_work* item)
{
    struct np_event* ev = CONTAINER_OF(item, struct np_event, work);
    handle_event(ev);
}

void handle_scheduled_event(struct k_work* item)
{
    struct k_work_delayable *dItem = k_work_delayable_from_work(item);
    struct np_event* ev = CONTAINER_OF(dItem, struct np_event, dWork);
    handle_event(ev);
}

void handle_event(struct np_event* ev)
{
    struct nabto_event_queue_zephyr* queue = ev->queue;
    bool lockCore = ev->lockCore;
    if (lockCore) {
        nabto_device_threads_mutex_lock(queue->coreMutex);
    }
    ev->cb(ev->cbData);
    if (lockCore) {
        nabto_device_threads_mutex_unlock(queue->coreMutex);
    }
}

np_error_code create_event(struct np_event_queue* obj, np_event_callback cb, void* cbData, struct np_event** event)
{
    struct np_event* ev = np_calloc(1, sizeof(struct np_event));
    if (ev == NULL) {
        return NABTO_EC_OUT_OF_MEMORY;
    }
    k_work_init(&ev->work, handle_work_event);
    k_work_init_delayable(&ev->dWork, handle_scheduled_event);

    ev->cb = cb;
    ev->cbData = cbData;

    ev->queue = obj->data;
    *event = ev;
    return NABTO_EC_OK;
}

void destroy_event(struct np_event* event)
{
    np_free(event);
}

void post_event(struct np_event* event)
{
    event->lockCore = true;
    int ec = k_work_submit(&event->work);
    if (ec == 0) {
        // Double post
        NABTO_LOG_ERROR(LOG, "Double posted event");
    }
    if (ec == 1 || ec == 2) {
        // ok
    } else {
        NABTO_LOG_ERROR(LOG, "k_work_submit failed %d", ec);
    }
}

bool post_event_maybe_double(struct np_event* event)
{
    event->lockCore = true;
    int ec = k_work_submit(&event->work);
    if (ec == 0) {
        return false;
    }
    if (ec == 1 || ec == 2) {
        return true;
    }
    // else some error occured
    NABTO_LOG_ERROR(LOG, "k_work_submit failed %d", ec);
    return false;
}

void post_event_no_core_lock(struct np_event* event)
{
    event->lockCore = false;
    int ec = k_work_submit(&event->work);
    if (ec == 0) {
        // Double post
        NABTO_LOG_ERROR(LOG, "Double posted event");
    }
    if (ec == 1 || ec == 2) {
        // ok
    } else {
        NABTO_LOG_ERROR(LOG, "k_work_submit failed %d", ec);
    }
}

void cancel_event(struct np_event* event)
{
    k_work_cancel(&event->work);
    k_work_cancel_delayable(&event->dWork);
}

void post_timed_event(struct np_event* event, uint32_t milliseconds)
{
    event->lockCore = true;
    int ec = k_work_schedule(&event->dWork, K_MSEC(milliseconds));
    if (ec == 0 || ec == 1) {
        // ok
    } else {
        NABTO_LOG_ERROR(LOG, "k_work_schedule failed %d", ec);
    }
}
