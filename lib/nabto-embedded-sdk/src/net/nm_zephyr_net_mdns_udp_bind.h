#ifndef _NM_ZEPHYR_NET_MDNS_UDP_BIND_H_
#define _NM_ZEPHYR_NET_MDNS_UDP_BIND_H_

#include <modules/mdns/nm_mdns_udp_bind.h>

struct nm_libevent_context;

struct nm_mdns_udp_bind nm_zephyr_net_mdns_udp_bind_get_impl(struct nm_zephyr_net* ctx);

#endif
