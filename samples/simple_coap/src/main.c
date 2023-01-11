#include <zephyr/net/net_if.h>
#include <zephyr/settings/settings.h>

#include <nabto/nabto_device_test.h>
#include <nabto/nabto_device_experimental.h>
#include <nabto/nabto_device_zephyr.h>

#if IS_ENABLED(CONFIG_SETTINGS_FS)
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#endif

#include <stdio.h>

LOG_MODULE_REGISTER(main);

#define STORAGE_PARTITION	storage_partition
#define STORAGE_PARTITION_ID	FIXED_PARTITION_ID(STORAGE_PARTITION)


static const char* coapPath[] = { "hello-world", NULL };
static const char* helloWorld = "Hello World from zephyr";

#define ASSERT_NABTO_EC(ec) __ASSERT(ec == NABTO_DEVICE_EC_OK, "Call failed %s", nabto_device_error_get_message(ec))

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

void main(void)
{
    init_fs();
    int status;
    status = settings_subsys_init();
    if (status != 0) {
        LOG_ERR("Cannot init settings %s", strerror(-status));
        return;
    }
    status = settings_load();
    __ASSERT(status == 0, "cannot load settings");


    struct net_if *iface;
    iface = net_if_get_default();

#if CONFIG_NET_DHCPV4
    net_dhcpv4_start(iface);
#endif

    NabtoDeviceError ec;

    NabtoDevice* device = nabto_device_zephyr_get_default_instance();

    __ASSERT(device != NULL, "Device is NULL");

    ec = nabto_device_set_product_id(device, CONFIG_SIMPLE_COAP_PRODUCT_ID);
    ASSERT_NABTO_EC(ec);

    ec = nabto_device_set_device_id(device, CONFIG_SIMPLE_COAP_DEVICE_ID);
    ASSERT_NABTO_EC(ec);

    uint8_t key[32];
    __ASSERT(strlen(CONFIG_SIMPLE_COAP_PRIVATE_KEY) == 64, "CONFIG_SIMPLE_COAP_PRIVATE_KEY should be 64 hex characters");
    size_t convertedLength = hex2bin(CONFIG_SIMPLE_COAP_PRIVATE_KEY, strlen(CONFIG_SIMPLE_COAP_PRIVATE_KEY), key, sizeof(key));
    __ASSERT(convertedLength == sizeof(key), "Could not parse the hex string CONFIG_SIMPLE_COAP_PRIVATE_KEY");
    ec = nabto_device_set_private_key_secp256r1(device, key, sizeof(key));

    ec = nabto_device_enable_mdns(device);
    ASSERT_NABTO_EC(ec);

    ec = nabto_device_add_server_connect_token(device, "demosct");
    ASSERT_NABTO_EC(ec);

    NabtoDeviceFuture* future = nabto_device_future_new(device);
    __ASSERT(future != NULL, "Future is null");

    nabto_device_start(device, future);
    ec = nabto_device_future_wait(future);
    ASSERT_NABTO_EC(ec);

    char* fp = NULL;
    ec = nabto_device_get_device_fingerprint(device, &fp);
    ASSERT_NABTO_EC(ec);

    LOG_INF("Nabto Device has been started with fingerprint %s", fp);
    nabto_device_string_free(fp);

    NabtoDeviceListener* listener = nabto_device_listener_new(device);
    __ASSERT(listener != NULL, "Listener is NULL");

    ec = nabto_device_coap_init_listener(device, listener, NABTO_DEVICE_COAP_GET, coapPath);
    ASSERT_NABTO_EC(ec);

    while(true)
    {
        NabtoDeviceCoapRequest* coapRequest;
        nabto_device_listener_new_coap_request(listener, future, &coapRequest);

        ec = nabto_device_future_wait(future);
        if (ec != NABTO_DEVICE_EC_OK) {
            break;
        } else
        {
            nabto_device_coap_response_set_code(coapRequest, 205);
            nabto_device_coap_response_set_content_format(coapRequest, NABTO_DEVICE_COAP_CONTENT_FORMAT_TEXT_PLAIN_UTF8);

            if (nabto_device_coap_response_set_payload(coapRequest, helloWorld, strlen(helloWorld)) == NABTO_DEVICE_EC_OK)
            {
                nabto_device_coap_response_ready(coapRequest);
            }
            LOG_INF("Responded to CoAP request\n");
            nabto_device_coap_request_free(coapRequest);
        }
    }

    nabto_device_listener_free(listener);
    nabto_device_future_free(future);
}
