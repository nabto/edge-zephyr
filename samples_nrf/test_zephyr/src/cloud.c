/* cloud.c - Copyright Svep Design Center AB */

/**** Includes ****/
#include <stdint.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dns_resolve.h>
#include <date_time.h>

#include <zephyr/net/net_if.h>
#include <zephyr/settings/settings.h>

#include <nabto/nabto_device_test.h>
#include <nabto/nabto_device_experimental.h>
#include <nabto/nabto_device_zephyr.h>
#include "cloud.h"

/**** Defines ****/
LOG_MODULE_REGISTER(cloud, LOG_LEVEL_DBG);

#define STORAGE_PARTITION	storage_partition
#define STORAGE_PARTITION_ID	FIXED_PARTITION_ID(STORAGE_PARTITION)

#define ASSERT_NABTO_EC(ec)                                                \
    do                                                                     \
    {                                                                      \
        if (ec != NABTO_DEVICE_EC_OK)                                      \
        {                                                                  \
            LOG_ERR("Call failed %s", nabto_device_error_get_message(ec)); \
            k_sleep(K_FOREVER);                                            \
        }                                                                  \
    } while (false)

// _ASSERT(ec == NABTO_DEVICE_EC_OK, "Call failed %s", nabto_device_error_get_message(ec))

/**** Types ****/


/**** Data ****/
static const char* coapPath[] = { "hello-world", NULL };
static const char* helloWorld = "Hello World from zephyr";

/**** Private functions ****/
static void init_fs()
{
#if IS_ENABLED(CONFIG_SETTINGS_FS)
    int rc;
    FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage);

    /* mounting info */
    static struct fs_mount_t littlefs_mnt = {
        .type = FS_LITTLEFS,
        .fs_data = &cstorage,
        .storage_dev = (void *)STORAGE_PARTITION_ID,
        .mnt_point = "/ff"};

    rc = fs_mount(&littlefs_mnt);
    if (rc != 0)
    {
        printk("mounting littlefs error: [%d]\n", rc);
    }
    else
    {

        rc = fs_unlink(CONFIG_SETTINGS_FS_FILE);
        if ((rc != 0) && (rc != -ENOENT))
        {
            printk("can't delete config file%d\n", rc);
        }
        else
        {
            printk("FS initialized: OK\n");
        }
    }
#endif
}

/**** Public functions ****/
int cloud_init()
{


    init_fs();
    int status;
    status = settings_subsys_init();
    if (status != 0)
    {
        LOG_ERR("Cannot init settings %s", strerror(-status));
        return status;
   }
   status = settings_load();
   __ASSERT(status == 0, "cannot load settings");
    struct net_if *iface;
    iface = net_if_get_default();

    if (iface==NULL)
    {
        LOG_ERR("iface is null");
    }
    else
    {
        LOG_INF("iface is not null");
    }

#if CONFIG_NET_DHCPV4
    net_dhcpv4_start(iface);
#endif

    NabtoDeviceError ec;

    //return 0;

    NabtoDevice* device = nabto_device_zephyr_get_default_instance();

    __ASSERT(device != NULL, "Device is NULL");
    nabto_device_set_log_level(NULL, "trace");

    ec = nabto_device_set_product_id(device, CONFIG_SIMPLE_COAP_PRODUCT_ID);
    ASSERT_NABTO_EC(ec);

    ec = nabto_device_set_device_id(device, CONFIG_SIMPLE_COAP_DEVICE_ID);
    ASSERT_NABTO_EC(ec);

    uint8_t key[32];
    __ASSERT(strlen(CONFIG_SIMPLE_COAP_PRIVATE_KEY) == 64, "CONFIG_SIMPLE_COAP_PRIVATE_KEY should be 64 hex characters");
    size_t convertedLength = hex2bin(CONFIG_SIMPLE_COAP_PRIVATE_KEY, strlen(CONFIG_SIMPLE_COAP_PRIVATE_KEY), key, sizeof(key));
    __ASSERT(convertedLength == sizeof(key), "Could  not parse the hex string CONFIG_SIMPLE_COAP_PRIVATE_KEY");
    ec = nabto_device_set_private_key_secp256r1(device, key, sizeof(key));
    //ec = nabto_device_enable_mdns(device);
    ASSERT_NABTO_EC(ec);
    ec = nabto_device_add_server_connect_token(device, "demosct");
    ASSERT_NABTO_EC(ec);

    NabtoDeviceFuture* future = nabto_device_future_new(device);
    nabto_device_start(device, future);
    ec = nabto_device_future_wait(future);
    ASSERT_NABTO_EC(ec);

    //char* fp = NULL;
    //ec = nabto_device_get_device_fingerprint(device, &fp);
    //ASSERT_NABTO_EC(ec);

    LOG_INF("Nabto device started w/o fingerprint, do we have that? \n");
    //nabto_device_string_free(fp);
    k_sleep(K_FOREVER);

    NabtoDeviceListener* listener = nabto_device_listener_new(device);
    __ASSERT(listener != NULL, "Listener is NULL");

    ec = nabto_device_coap_init_listener(device, listener, NABTO_DEVICE_COAP_GET, coapPath);
    ASSERT_NABTO_EC(ec);


    while(true)
    {
        NabtoDeviceCoapRequest* coapRequest;
        nabto_device_listener_new_coap_request(listener, future, &coapRequest);
        LOG_INF("Listener_new_coap_request: %d \n", ec);
        ec = nabto_device_future_wait(future);
        LOG_INF("Future wait: %d \n", ec);

        LOG_INF("check status: %d \n", ec);

        if (ec != NABTO_DEVICE_EC_OK) {
            LOG_INF("error code: %d \n", ec);
            break;
        } else
        {
            nabto_device_coap_response_set_code(coapRequest, 205);
            LOG_INF("Set code: %d \n", ec);

            nabto_device_coap_response_set_content_format(coapRequest, NABTO_DEVICE_COAP_CONTENT_FORMAT_TEXT_PLAIN_UTF8);
            LOG_INF("Set content: %d \n", ec);

            if (nabto_device_coap_response_set_payload(coapRequest, helloWorld, strlen(helloWorld)) == NABTO_DEVICE_EC_OK)
            {
                LOG_INF("Set payload: %d \n", ec);

                nabto_device_coap_response_ready(coapRequest);
            }
            LOG_INF("Responded to CoAP request\n");
            nabto_device_coap_request_free(coapRequest);
        }
    }

    nabto_device_listener_free(listener);
    nabto_device_future_free(future);
    return 0;
}
