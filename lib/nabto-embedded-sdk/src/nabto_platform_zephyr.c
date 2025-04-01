#include <api/nabto_device_platform.h>

#include <platform/np_error_code.h>
#include <platform/np_allocator.h>

#include <api/nabto_device_defines.h>
#include <api/nabto_device_integration.h>

#include <net/nm_zephyr_net.h>
#include <net/nm_zephyr_net_mdns_udp_bind.h>
#include <zephyr/net/net_mgmt.h>

#include "nabto_event_queue_zephyr.h"
#include <ts/nm_zephyr_timestamp.h>
#include <dns/nm_zephyr_dns.h>

#include <modules/mdns/nm_mdns_server.h>



#define LOG NABTO_LOG_MODULE_PLATFORM


struct nabto_device_platform_zephyr {
    struct nm_zephyr_net netCtx;
    struct nabto_event_queue_zephyr eventQueueZephyr;
    struct nm_mdns_server mdnsServer;
    struct net_mgmt_event_callback mgmtCallback;
};


#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
static void l4_event_handler(struct net_mgmt_event_callback *cb,
                             uint32_t event,
                             struct net_if *iface)
{
    struct nabto_device_platform_zephyr *platform =
		CONTAINER_OF(cb, struct nabto_device_platform_zephyr, mgmtCallback);

    switch (event)
    {
    case NET_EVENT_L4_CONNECTED:
        NABTO_LOG_INFO(LOG, "Network connectivity established");
        nm_zephyr_net_up(&platform->netCtx);
        break;
    case NET_EVENT_L4_DISCONNECTED:
        NABTO_LOG_INFO(LOG, "Network connectivity lost");
        break;
    default:
        /* Don't care */
        return;
    }
}

np_error_code nabto_device_platform_init(struct nabto_device_context* device, struct nabto_device_mutex* coreMutex)
{
    struct nabto_device_platform_zephyr* platformAdapter = np_calloc(1, sizeof(struct nabto_device_platform_zephyr));
    if (platformAdapter == NULL) {
        return NABTO_EC_OUT_OF_MEMORY;
    }
    nabto_device_integration_set_platform_data(device, platformAdapter);


    np_error_code ec = nabto_event_queue_zephyr_init(&platformAdapter->eventQueueZephyr, coreMutex);
    if (ec != NABTO_EC_OK) {
        return ec;
    }

    ec = nm_zephyr_net_init(&platformAdapter->netCtx);
    if (ec != NABTO_EC_OK) {
        return ec;
    }

    nm_zephyr_net_run(&platformAdapter->netCtx);

    struct np_udp udp = nm_zephyr_net_udp_get_impl(&platformAdapter->netCtx);
    struct np_tcp tcp = nm_zephyr_net_tcp_get_impl(&platformAdapter->netCtx);
    struct np_event_queue eq = nabto_event_queue_zephyr_get_impl(&platformAdapter->eventQueueZephyr);
    struct np_timestamp timestamp = nm_zephyr_ts_get_impl();
    struct np_dns dns = nm_zephyr_dns_get_impl();

    struct np_local_ip localIp = nm_zephyr_net_local_ip_get_impl(&platformAdapter->netCtx);

#if defined(NET_ROUTE_MCAST)

    struct nm_mdns_udp_bind mdnsUdpBind = nm_zephyr_net_mdns_udp_bind_get_impl(&platformAdapter->netCtx);
    ec = nm_mdns_server_init(&platformAdapter->mdnsServer, &eq, &udp, &mdnsUdpBind, &localIp);
    if (ec != NABTO_EC_OK) {
        NABTO_LOG_ERROR(LOG, "Could not start mdns server");
        return ec;
    }
#endif

    //struct np_mdns mdnsImpl = nm_mdns_server_get_impl(&platformAdapter->mdnsServer);

    nabto_device_integration_set_udp_impl(device, &udp);
    nabto_device_integration_set_tcp_impl(device, &tcp);
    nabto_device_integration_set_timestamp_impl(device, &timestamp);
    nabto_device_integration_set_dns_impl(device, &dns);
    nabto_device_integration_set_event_queue_impl(device, &eq);
    nabto_device_integration_set_local_ip_impl(device, &localIp);
    //nabto_device_integration_set_mdns_impl(device, &mdnsImpl);

    net_mgmt_init_event_callback(&platformAdapter->mgmtCallback, l4_event_handler, L4_EVENT_MASK);
    net_mgmt_add_event_callback(&platformAdapter->mgmtCallback);

    return NABTO_EC_OK;
}

void nabto_device_platform_deinit(struct nabto_device_context* device)
{
    struct nabto_device_platform_zephyr *platformAdapter = (struct nabto_device_platform_zephyr *)device->platformAdapter;
    net_mgmt_del_event_callback(&platformAdapter->mgmtCallback);
    np_free(platformAdapter);
}

void nabto_device_platform_close(struct nabto_device_context* device, struct np_completion_event* closeEvent)
{
    // do nothing as mdns is handled by zephyr
}

void nabto_device_platform_stop_blocking(struct nabto_device_context* device)
{
    // TODO stop queues
}
