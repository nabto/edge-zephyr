#include <nabto/nabto_device.h>

#include "nabto_device_zephyr_internal.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nabto, LOG_LEVEL_DBG);

static void nabto_log_output(NabtoDeviceLogMessage* msg, void* data);

void nabto_log_output(NabtoDeviceLogMessage* msg, void* data)
{
    if (msg->severity == NABTO_DEVICE_LOG_ERROR) {
        LOG_ERR("%s", msg->message);
    } else if (msg->severity == NABTO_DEVICE_LOG_INFO) {
        LOG_INF("%s", msg->message);
    } else if (msg->severity == NABTO_DEVICE_LOG_WARN) {
        LOG_WRN("%s", msg->message);
    } else if (msg->severity == NABTO_DEVICE_LOG_TRACE) {
        LOG_DBG("%s", msg->message);
    }
}

void nabto_device_zephyr_internal_init_logging()
{
    static bool loggingInitialized = false;
    if (!loggingInitialized) {
        nabto_device_set_log_callback(NULL, nabto_log_output, NULL);
        nabto_device_set_log_level(NULL, "info");
    }
}

NabtoDevice* nabto_device_zephyr_get_default_instance()
{
    static bool initialized = false;
    static NabtoDevice* device = NULL;
    if (!initialized) {
        nabto_device_zephyr_internal_init_logging();
        device = nabto_device_new();
        initialized = true;
    }
    return device;
}
