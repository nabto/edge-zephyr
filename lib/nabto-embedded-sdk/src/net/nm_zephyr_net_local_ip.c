#include <platform/np_logging.h>
#include <platform/interfaces/np_local_ip.h>

#include <zephyr/net/socket.h>

#include "nm_zephyr_net.h"

#define LOG NABTO_LOG_MODULE_UDP

static size_t get_local_ips(struct np_local_ip* obj, struct np_ip_address *addrs, size_t addrsSize);


const struct np_local_ip_functions module = {
    .get_local_ips = get_local_ips
};

struct np_local_ip nm_zephyr_net_local_ip_get_impl(struct nm_zephyr_net* ctx)
{
    struct np_local_ip obj;
    obj.mptr = &module;
    obj.data = ctx;
    return obj;
}

size_t get_local_ips(struct np_local_ip* obj, struct np_ip_address *addrs, size_t addrsSize)
{
    size_t ind = 0;

    {
        struct net_if *iface;
        iface = net_if_get_default();
        struct in_addr* v4Addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);

        if (v4Addr && addrsSize > 0)
        {
            struct np_ip_address *addr = &addrs[ind];
            addr->type = NABTO_IPV4;
            memcpy(addr->ip.v4, v4Addr->s4_addr, 4);
            ind++;
            addrsSize--;
        }
    }

    {
        struct net_if *iface = NULL;
        struct in6_addr *v6Addr = net_if_ipv6_get_global_addr(NET_ADDR_PREFERRED, &iface);

        if (v6Addr && addrsSize > 0)
        {
            struct np_ip_address *addr = &addrs[ind];
            addr->type = NABTO_IPV6;
            memcpy(addr->ip.v6, v6Addr->s6_addr, 16);
            ind++;
            addrsSize--;
        }

    }
    return ind;
}
