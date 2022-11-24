#ifndef _NABTO_EVENT_QUEUE_ZEPHYR_H_
#define _NABTO_EVENT_QUEUE_ZEPHYR_H_

#include <api/nabto_device_threads.h>

/**
 * Since we are running everything from one thread we can directly use
 * the nm_event_queue module without any custom locking on event
 * posting. When executing events we need to have the appropriate
 * locks such that the core of the device is synchronized with the
 * application which uses the nabto_device.h api.
 *
 * We do however need to notify the select platform when events or
 * timed events is posted to the event queue such that they can be
 * handled if they are posted from the outside.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct nm_event_queue;
struct np_platform;

struct nabto_event_queue_zephyr {
    // coreMutex is used to synchronize all access to the core of the
    // platform. The coreMutex needs to be taken before event
    // callbacks is executed.
    struct nabto_device_mutex* coreMutex;

    // The queueMutex protects the queue, The queue post function can
    // be called without the coreMutex being taken.
    bool stopped;
};

np_error_code nabto_event_queue_zephyr_init(struct nabto_event_queue_zephyr* queue, struct nabto_device_mutex* coreMutex);

void nabto_event_queue_zephyr_deinit(struct nabto_event_queue_zephyr* queue);

// start the thread
//np_error_code thread_event_queue_run(struct thread_event_queue* queue);

struct np_event_queue nabto_event_queue_zephyr_get_impl(struct nabto_event_queue_zephyr* queue);

//bool thread_event_queue_do_one(struct thread_event_queue* queue);

//void thread_event_queue_stop_blocking(struct thread_event_queue* queue);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
