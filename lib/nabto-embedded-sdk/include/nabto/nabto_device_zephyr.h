#ifndef _NABTO_DEVICE_ZEPHYR_H_
#define _NABTO_DEVICE_ZEPHYR_H_

#include <nabto/nabto_device.h>

#include <modules/file/nm_file.h>

NabtoDevice* nabto_device_zephyr_get_default_instance();

NabtoDeviceError nabto_device_zephyr_reset_private_key();

NabtoDeviceError nabto_device_zephyr_load_private_key(NabtoDevice* device);

struct nm_file nm_file_zephyr_get_impl();

#endif
