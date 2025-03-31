#include "net/nm_zephyr_net_udp.h"
#include "zephyr/sys/atomic.h"

static atomic_t conn = ATOMIC_INIT(true);

void nm_zephyr_set_conn(bool is_conn) {
    atomic_set(&conn, is_conn);

    if (is_conn) {
        nm_zephyr_net_recreate_udp();
    } else {
        nm_zephyr_net_down();
    }
}

bool nm_zephyr_get_conn(void) {
    return atomic_get(&conn);
}
