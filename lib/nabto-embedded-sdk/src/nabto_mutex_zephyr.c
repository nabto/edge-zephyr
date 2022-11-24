#include <api/nabto_device_threads.h>

#include <zephyr/kernel.h>
#include <platform/np_allocator.h>

struct nabto_device_mutex {
    struct k_mutex mutex;
};

struct nabto_device_condition {
    struct k_condvar cond;
};


struct nabto_device_mutex* nabto_device_threads_create_mutex(void)
{
    struct nabto_device_mutex* mutex = np_calloc(1, sizeof(struct nabto_device_mutex));
    if (mutex == NULL) {
        return NULL;
    }

    int ec = k_mutex_init(&mutex->mutex);
    if (ec != 0) {
        np_free(mutex);
        return NULL;
    }
    return mutex;
}


struct nabto_device_condition* nabto_device_threads_create_condition(void)
{
    struct nabto_device_condition* cond = np_calloc(1, sizeof(struct nabto_device_condition));
    if (cond == NULL) {
        return NULL;
    }
    int ec = k_condvar_init(&cond->cond);
    if (ec != 0) {
        np_free(cond);
        return NULL;
    }
    return cond;
}

void nabto_device_threads_free_mutex(struct nabto_device_mutex* mutex)
{
    np_free(mutex);
}

void nabto_device_threads_free_cond(struct nabto_device_condition* cond)
{
    np_free(cond);
}

void nabto_device_threads_mutex_lock(struct nabto_device_mutex* mutex)
{
    int ec = k_mutex_lock(&mutex->mutex, K_FOREVER);
    if (ec != 0) {
        // TODO mutex was not locked
    }
}
void nabto_device_threads_mutex_unlock(struct nabto_device_mutex* mutex)
{
     int ec = k_mutex_unlock(&mutex->mutex);
    if (ec != 0) {
        // TODO mutex was not locked
    }
}

void nabto_device_threads_cond_signal(struct nabto_device_condition* cond)
{
    k_condvar_signal(&cond->cond);
}
void nabto_device_threads_cond_wait(struct nabto_device_condition* cond,
                                    struct nabto_device_mutex* mutex)
{
    int ec = k_condvar_wait(&cond->cond, &mutex->mutex, K_FOREVER);
    if (ec != 0) {
        // TODO
    }
}
void nabto_device_threads_cond_timed_wait(struct nabto_device_condition* cond,
                                          struct nabto_device_mutex* mutex,
                                          uint32_t ms)
{
    int ec = k_condvar_wait(&cond->cond, &mutex->mutex, K_MSEC(ms));
    if (ec != 0) {
        // TODO
    }
}
