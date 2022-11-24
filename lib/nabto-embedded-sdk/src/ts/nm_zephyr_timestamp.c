#include "nm_zephyr_timestamp.h"

#include <stdint.h>

#include <zephyr/kernel.h>

static uint32_t ts_now_ms(struct np_timestamp* obj);

static struct np_timestamp_functions module = {
    .now_ms               = &ts_now_ms
};

struct np_timestamp nm_zephyr_ts_get_impl()
{
    struct np_timestamp ts;
    ts.mptr = &module;
    ts.data = NULL;
    return ts;
}

uint32_t ts_now_ms(struct np_timestamp* obj)
{
    return k_uptime_get_32();
}
