#ifndef _NP_CONFIG_ZEPHYR_H_
#define _NP_CONFIG_ZEPHYR_H_

#include <zephyr/kernel.h>

#define NP_ALLOCATOR_FREE k_free
#define NP_ALLOCATOR_CALLOC k_calloc

#endif
