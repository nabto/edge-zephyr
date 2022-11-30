#include <zephyr/net/net_if.h>

#include <nabto/nabto_device_test.h>

#include <stdio.h>
LOG_MODULE_REGISTER(main);

#define ASSERT_NABTO_EC(ec) __ASSERT(ec == NABTO_DEVICE_EC_OK, "Call failed %s", nabto_device_error_get_message(ec))

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

void main(void) {

    char buffer[42];
    memset(buffer, 0, 42);
    sprintf(buffer, "%" PRIu32, (uint32_t)42);
    LOG_INF("%s", buffer);

    struct net_if *iface;
    iface = net_if_get_default();
    net_dhcpv4_start(iface);

    ASSERT_NABTO_EC(nabto_device_set_log_callback(NULL, nabto_log_output, NULL));
    ASSERT_NABTO_EC(nabto_device_set_log_level(NULL, "trace"));

    NabtoDevice* device = nabto_device_new();
    __ASSERT(device != NULL, "Device is NULL");

    ASSERT_NABTO_EC(nabto_device_set_product_id(device, "pr-foo"));
    ASSERT_NABTO_EC(nabto_device_set_device_id(device, "de-bar"));

    char* key = NULL;
    ASSERT_NABTO_EC(nabto_device_create_private_key(device, &key));

    ASSERT_NABTO_EC(nabto_device_set_private_key(device, key));
    nabto_device_string_free(key);

    NabtoDeviceFuture* future = nabto_device_future_new(device);
    __ASSERT(future != NULL, "Future is null");

    nabto_device_start(device, future);
    ASSERT_NABTO_EC(nabto_device_future_wait(future));
    nabto_device_future_free(future);

    char* fp = NULL;
    nabto_device_get_device_fingerprint(device, &fp);

    LOG_INF("Nabto Device has been started with fingerprint %s", fp);
    nabto_device_string_free(fp);
}
