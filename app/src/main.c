#include <zephyr/net/net_if.h>

#include <nabto/nabto_device_test.h>

#include <stdio.h>

LOG_MODULE_REGISTER(main);

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
    struct net_if *iface;
    iface = net_if_get_default();
    net_dhcpv4_start(iface);

    // wait for dhcp
    k_sleep(K_MSEC(4000));



    nabto_device_set_log_callback(NULL, nabto_log_output, NULL);
    nabto_device_set_log_level(NULL, "trace");

    NabtoDevice* device = nabto_device_test_new();
    if (device == NULL) {
        LOG_ERR("device == NULL");
    } else {
        NabtoDeviceFuture* future = nabto_device_future_new(device);
        nabto_device_test_dns(device, future);
        NabtoDeviceError ec = nabto_device_future_wait(future);
        if (ec == NABTO_DEVICE_EC_OK) {
            LOG_INF("dns test passed");
        } else {
            LOG_INF("dns test failed");
        }

        nabto_device_test_future_resolve(device, future);
        ec = nabto_device_future_wait(future);
        if (ec == NABTO_DEVICE_EC_OK) {
            LOG_INF("future resolve test passed");
        } else {
            LOG_INF("future resolve test failed");
        }

        nabto_device_test_event_queue(device, future);
        ec = nabto_device_future_wait(future);
        if (ec == NABTO_DEVICE_EC_OK) {
            LOG_INF("event queue test passed");
        } else {
            LOG_INF("event queue test failed");
        }

        nabto_device_test_logging(device);

        uint32_t ts1;
        uint32_t ts2;
        nabto_device_test_timestamp(device, &ts1);
        k_sleep(K_MSEC(1000));
        nabto_device_test_timestamp(device, &ts2);
        LOG_INF("Timestamp difference should be ~1000, %d", ts2 - ts1);

        nabto_device_test_udp(device, "192.168.1.163", 1235, future);
        ec = nabto_device_future_wait(future);
        if (ec == NABTO_DEVICE_EC_OK) {
            LOG_INF("udp test passed");
        } else {
            LOG_INF("udp test failed");
        }

        nabto_device_future_free(future);
        nabto_device_test_free(device);
    }
    LOG_INF("test end");
}
