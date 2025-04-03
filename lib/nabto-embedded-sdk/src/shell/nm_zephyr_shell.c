#include <zephyr/shell/shell.h>

#include <nabto/nabto_device.h>
#include <nabto/nabto_device_test.h>
#include <nabto/nabto_device_zephyr.h>
#include <nabto/nabto_device_experimental.h>

#include "nabto_device_zephyr_internal.h"

#include <stdlib.h>

LOG_MODULE_DECLARE(nabto);

static int cmd_nabto_test_udp(const struct shell *shell, size_t argc, char **argv, void *data);
static int cmd_nabto_test_tcp(const struct shell *shell, size_t argc, char **argv, void *data);
static int cmd_nabto_test_logging(const struct shell *shell, size_t argc, char **argv, void *data);
static int cmd_nabto_test_future_resolve(const struct shell *shell, size_t argc, char **argv, void *data);
static int cmd_nabto_test_event_queue(const struct shell *shell, size_t argc, char **argv, void *data);
static int cmd_nabto_test_timestamp(const struct shell *shell, size_t argc, char **argv, void *data);
static int cmd_nabto_test_dns(const struct shell *shell, size_t argc, char **argv, void *data);
static int cmd_nabto_test_local_ip(const struct shell *shell, size_t argc, char **argv, void *data);
static int cmd_nabto_test_mdns_publish(const struct shell *shell, size_t argc, char **argv, void *data);

static int cmd_nabto_log_level_set(const struct shell *shell, size_t argc, char **argv, void *data);

static int cmd_nabto_status(const struct shell *shell, size_t argc, char **argv, void *data);

static int cmd_nabto_crypto_speedtest(const struct shell *shell, size_t argc, char **argv, void *data);


static NabtoDevice* get_default_test_device();

SHELL_STATIC_SUBCMD_SET_CREATE(test_nabto,
        SHELL_CMD_ARG(udp, NULL, "Test platform UDP integration. Args ip, port.", cmd_nabto_test_udp, 3, 0),
        SHELL_CMD_ARG(tcp, NULL, "Test platform TCP integration. Args ip, port.", cmd_nabto_test_tcp, 3, 0),
        SHELL_CMD(logging, NULL, "Test platform logging integration.", cmd_nabto_test_logging),
        SHELL_CMD(future_resolve, NULL, "Test platform future resolve integration.", cmd_nabto_test_future_resolve),
        SHELL_CMD(event_queue, NULL, "Test platform event queue integration.", cmd_nabto_test_event_queue),
        SHELL_CMD(timestamp, NULL, "Test platform timestamp integration.", cmd_nabto_test_timestamp),
        SHELL_CMD(dns, NULL, "Test platform dns integration.", cmd_nabto_test_dns),
        SHELL_CMD(local_ip, NULL, "Test platform local ip integration.", cmd_nabto_test_local_ip),
        SHELL_CMD(mdns_publish, NULL, "Test platform publish of mdns service.", cmd_nabto_test_mdns_publish),
        SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(log_nabto,
    SHELL_CMD_ARG(level, NULL, "Set Log level", cmd_nabto_log_level_set, 2,0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_nabto,
        SHELL_CMD(test, &test_nabto, "Test Nabto integration.", NULL),
        SHELL_CMD(log, &log_nabto, "Nabto Log Settings.", NULL),
        SHELL_CMD(status, NULL, "Nabto Status.", cmd_nabto_status),
        SHELL_CMD(crypto_speedtest, NULL, "Crypto speedtest.", cmd_nabto_crypto_speedtest),
        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(nabto, &sub_nabto, "Nabto commands", NULL);

static int cmd_nabto_test_udp(const struct shell *shell, size_t argc, char **argv, void *data)
{
    shell_print(shell, "test udp %s", argv[1]);
    const char* host = argv[1];
    int port = atoi(argv[2]);

    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        NabtoDeviceFuture* future = nabto_device_future_new(instance);

        nabto_device_test_udp(instance, host, port, future);
        NabtoDeviceError ec = nabto_device_future_wait(future);
        if (ec == NABTO_DEVICE_EC_OK) {
            shell_print(shell, "udp test passed");
        } else {
            shell_print(shell, "udp test failed");
        }
        nabto_device_future_free(future);
    }

    return 0;
}

static int cmd_nabto_test_tcp(const struct shell *shell, size_t argc, char **argv, void *data)
{
    const char* host = argv[1];
    int port = atoi(argv[2]);

    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        NabtoDeviceFuture* future = nabto_device_future_new(instance);

        nabto_device_test_tcp(instance, host, port, future);
        NabtoDeviceError ec = nabto_device_future_wait(future);
        if (ec == NABTO_DEVICE_EC_OK) {
            shell_print(shell, "tcp test passed");
        } else {
            shell_print(shell, "tcp test failed");
        }
        nabto_device_future_free(future);
    }

    return 0;
}

static int cmd_nabto_test_logging(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        shell_print(shell, "test passes if appropriate logoutput is shown.");
        nabto_device_test_logging(instance);
    }
    return 0;
}

static int cmd_nabto_test_future_resolve(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        NabtoDeviceFuture* future = nabto_device_future_new(instance);
        nabto_device_test_future_resolve(instance, future);
        NabtoDeviceError ec = nabto_device_future_wait(future);
        nabto_device_future_free(future);
        if (ec != NABTO_DEVICE_EC_OK) {
            shell_print(shell, "test failed");
        } else {
            shell_print(shell, "test passed");
        }
    }
    return 0;
}

static int cmd_nabto_test_event_queue(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        NabtoDeviceFuture* future = nabto_device_future_new(instance);
        nabto_device_test_event_queue(instance, future);
        NabtoDeviceError ec = nabto_device_future_wait(future);
        nabto_device_future_free(future);
        if (ec != NABTO_DEVICE_EC_OK) {
            shell_print(shell, "test failed");
        } else {
            shell_print(shell, "test passed");
        }
    }
    return 0;
}

static int cmd_nabto_test_timestamp(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        uint32_t ts1;
        uint32_t ts2;
        nabto_device_test_timestamp(instance, &ts1);
        k_sleep(K_MSEC(1000));
        nabto_device_test_timestamp(instance, &ts2);
        shell_print(shell, "Timestamp difference should be ~1000, %d", ts2 - ts1);
    }
    return 0;
}

static int cmd_nabto_test_dns(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        NabtoDeviceFuture* future = nabto_device_future_new(instance);
        nabto_device_test_dns(instance, future);
        NabtoDeviceError ec = nabto_device_future_wait(future);
        nabto_device_future_free(future);
        if (ec != NABTO_DEVICE_EC_OK) {
            shell_print(shell, "test failed");
        } else {
            shell_print(shell, "test passed");
        }
    }
    return 0;
}

static int cmd_nabto_test_local_ip(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        nabto_device_test_local_ip(instance);
    }
    return 0;
}

static int cmd_nabto_test_mdns_publish(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* instance = get_default_test_device();
    if (instance == NULL) {
        shell_print(shell, "device instance == NULL");
    } else {
        nabto_device_test_mdns_publish_service(instance);
    }
    return 0;
}

static int cmd_nabto_log_level_set(const struct shell *shell, size_t argc, char **argv, void *data)
{
    const char* logLevel = argv[1];

    NabtoDeviceError ec = nabto_device_set_log_level(NULL, logLevel);
    if (ec == NABTO_DEVICE_EC_OK) {
        shell_print(shell, "Set log level to %s", logLevel);
    } else {
        shell_print(shell, "Could not set log level to %s", logLevel);
    }
    return 0;
}

static int cmd_nabto_status(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* device = nabto_device_zephyr_get_default_instance();
    if (device == NULL) {
        shell_print(shell, "A Nabto Device object could not be obtained");
    } else {
        const char* productId = nabto_device_get_product_id(device);
        const char* deviceId = nabto_device_get_device_id(device);
        char* fp = NULL;
        nabto_device_get_device_fingerprint(device, &fp);
        shell_print(shell, "Product id: %s", productId);
        shell_print(shell, "Device id: %s", deviceId);
        if (fp != NULL) {
            shell_print(shell, "Device fingerprint: %s", fp);
        }
    }
    return 0;
}

static int cmd_nabto_crypto_speedtest(const struct shell *shell, size_t argc, char **argv, void *data)
{
    NabtoDevice* device = nabto_device_zephyr_get_default_instance();
    if (device == NULL) {
        shell_print(shell, "A Nabto Device object could not be obtained");
    } else {
        NabtoDeviceError ec = nabto_device_crypto_speed_test(device);
        if (ec != NABTO_DEVICE_EC_OK) {
            shell_print(shell, "crypto speed test failed %s", nabto_device_error_get_message(ec));
        }
    }
    return 0;
}


static NabtoDevice* testDevice = NULL;

NabtoDevice* get_default_test_device()
{
    static bool initialized = false;
    if (!initialized) {
        nabto_device_zephyr_internal_init_logging();
        testDevice = nabto_device_test_new();
        if (testDevice == NULL) {
            LOG_ERR("Could not create a test device instance");
        }
        initialized = true;
    }
    return testDevice;
}
