#include <nabto/nabto_device_zephyr.h>

#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#include <platform/np_allocator.h>

static char* globalPrivateKey = NULL;

LOG_MODULE_DECLARE(nabto);

static int private_key_set(const char *name, size_t len_rd,
		       settings_read_cb read_cb, void *cb_arg)
{
	ssize_t len;

    if (globalPrivateKey != NULL) {
        np_free(globalPrivateKey);
        globalPrivateKey = NULL;
    }

    globalPrivateKey = np_calloc(1, len_rd+1);
    if (globalPrivateKey == NULL) {
        return 0;
    }

	len = read_cb(cb_arg, globalPrivateKey, len_rd);
	if (len != len_rd) {
		LOG_ERR("Failed to decode nabto/private_key (err %zd)", len);
		return len;
	}

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(nabto_private_key, "nabto/private_key", NULL, private_key_set, NULL, NULL);

NabtoDeviceError nabto_device_zephyr_reset_private_key()
{
    NabtoDevice* device = nabto_device_zephyr_get_default_instance();
    if (device == NULL) {
        return NABTO_DEVICE_EC_UNKNOWN;
    }
    char* privateKey = NULL;
    NabtoDeviceError ec = nabto_device_create_private_key(device, &privateKey);
    if (ec == NABTO_DEVICE_EC_OK) {
        np_free(globalPrivateKey);
        size_t privateKeyLength = strlen(privateKey);
        int error = settings_save_one("nabto/private_key", privateKey, privateKeyLength);
        if (error != 0) {
            ec = NABTO_DEVICE_EC_UNKNOWN;
        } else {
            globalPrivateKey = np_calloc(1, privateKeyLength + 1);
            if (globalPrivateKey == NULL) {
                ec = NABTO_DEVICE_EC_OUT_OF_MEMORY;
            } else {
                memcpy(globalPrivateKey, privateKey, privateKeyLength);
            }
        }
    }
    nabto_device_string_free(privateKey);
    return ec;
}

NabtoDeviceError nabto_device_zephyr_load_private_key(NabtoDevice* device)
{
    NabtoDeviceError ec;
    if (globalPrivateKey == NULL) {
        ec = nabto_device_zephyr_reset_private_key();
        if (ec != NABTO_DEVICE_EC_OK) {
            return ec;
        }
    }
    if (globalPrivateKey != NULL) {
        ec = nabto_device_set_private_key(device, globalPrivateKey);
    } else {
        ec = NABTO_DEVICE_EC_OUT_OF_MEMORY;
    }
    return ec;
}
