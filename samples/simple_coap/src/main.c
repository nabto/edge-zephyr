#include <zephyr/net/net_if.h>

#include <nabto/nabto_device_test.h>
#include <nabto/nabto_device_zephyr.h>

#include <stdio.h>
LOG_MODULE_REGISTER(main);

static const char* coapPath[] = { "hello-world", NULL };
static const char* helloWorld = "Hello World from zephyr";

#define ASSERT_NABTO_EC(ec) __ASSERT(ec == NABTO_DEVICE_EC_OK, "Call failed %s", nabto_device_error_get_message(ec))

void main(void)
{
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

    char* key = NULL;
    ec = nabto_device_create_private_key(device, &key);
    ASSERT_NABTO_EC(ec);

    ec = nabto_device_set_private_key(device, key);
    ASSERT_NABTO_EC(ec);

    nabto_device_string_free(key);

    ec = nabto_device_enable_mdns(device);
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
            printf("Responded to CoAP request\n");
            nabto_device_coap_request_free(coapRequest);
        }
    }

    nabto_device_listener_free(listener);
    nabto_device_future_free(future);
}
