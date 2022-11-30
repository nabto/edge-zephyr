#include "nm_zephyr_net_udp.h"

#include <platform/np_logging.h>
#include <platform/np_util.h>
#include <platform/np_completion_event.h>

#include <modules/mdns/nm_mdns_udp_bind.h>


#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/igmp.h>

// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <errno.h>
// #include <string.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <arpa/inet.h>
// #include <net/if.h>
// #include <ifaddrs.h>

#define LOG NABTO_LOG_MODULE_UDP

static void nm_zephyr_net_async_bind_mdns_ipv4(struct np_udp_socket* sock, struct np_completion_event* completionEvent);
static void nm_zephyr_net_async_bind_mdns_ipv6(struct np_udp_socket* sock, struct np_completion_event* completionEvent);
static np_error_code create_socket_ipv6(struct np_udp_socket* s);
static np_error_code create_socket_ipv4(struct np_udp_socket* s);
static bool init_mdns_ipv6_socket(int sock);
static bool init_mdns_ipv4_socket(int sock);
static void mdns_update_ipv4_socket_registration(int sock);
static void mdns_update_ipv6_socket_registration(int sock);


static struct nm_mdns_udp_bind_functions module = {
    .async_bind_mdns_ipv4 = &nm_zephyr_net_async_bind_mdns_ipv4,
    .async_bind_mdns_ipv6 = &nm_zephyr_net_async_bind_mdns_ipv6
};

struct nm_mdns_udp_bind nm_zephyr_net_mdns_udp_bind_get_impl(struct nm_zephyr_net* ctx)
{
    struct nm_mdns_udp_bind obj;
    obj.mptr = &module;
    obj.data = ctx;
    return obj;
}


np_error_code nm_zephyr_net_async_bind_mdns_ipv4_ec(struct np_udp_socket* sock)
{
    if (sock->aborted) {
        NABTO_LOG_ERROR(LOG, "bind called on aborted socket");
        return NABTO_EC_ABORTED;
    }

    np_error_code ec;
    ec = create_socket_ipv4(sock);

    if (ec != NABTO_EC_OK) {
        return ec;
    }

    if (!init_mdns_ipv4_socket(sock->sock)) {
        NABTO_LOG_ERROR(LOG, "init_mdns_ipv4_socket failed");
        zsock_close(sock->sock);
        return NABTO_EC_UDP_SOCKET_CREATION_ERROR;
    }

    mdns_update_ipv4_socket_registration(sock->sock);

    return NABTO_EC_OK;
}

void nm_zephyr_net_async_bind_mdns_ipv4(struct np_udp_socket* sock, struct np_completion_event* completionEvent)
{
    np_error_code ec = nm_zephyr_net_async_bind_mdns_ipv4_ec(sock);
    np_completion_event_resolve(completionEvent, ec);
}

np_error_code nm_zephyr_net_async_bind_mdns_ipv6_ec(struct np_udp_socket* sock)
{
    if (sock->aborted) {
        NABTO_LOG_ERROR(LOG, "bind called on aborted socket");
        return NABTO_EC_ABORTED;
    }

    np_error_code ec = create_socket_ipv6(sock);
    if (ec) {
        return ec;
    }

    int no = 0;
    int status = zsock_setsockopt(sock->sock, IPPROTO_IPV6, IPV6_V6ONLY, (void* ) &no, sizeof(no));
    if (status < 0)
    {
        NABTO_LOG_ERROR(LOG, "Cannot set IPV6_V6ONLY");
    }

    if (!init_mdns_ipv6_socket(sock->sock)) {
        zsock_close(sock->sock);
        return NABTO_EC_UDP_SOCKET_CREATION_ERROR;
    }

    mdns_update_ipv6_socket_registration(sock->sock);

    return NABTO_EC_OK;
}

void nm_zephyr_net_async_bind_mdns_ipv6(struct np_udp_socket* sock, struct np_completion_event* completionEvent)
{
    np_error_code ec = nm_zephyr_net_async_bind_mdns_ipv6_ec(sock);
    np_completion_event_resolve(completionEvent, ec);
}


np_error_code create_socket_ipv6(struct np_udp_socket* s)
{
    int sock = nm_zephyr_net_udp_nonblocking_socket(AF_INET6, SOCK_DGRAM);
    if (sock == -1) {
        return NABTO_EC_UDP_SOCKET_CREATION_ERROR;
    }

    int no = 0;
    int status = zsock_setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (void* ) &no, sizeof(no));
    if (status < 0) {
        NABTO_LOG_ERROR(LOG, "Cannot set IPV6_V6ONLY");
    }

    s->type = NABTO_IPV6;
    s->sock = sock;
    return NABTO_EC_OK;
}


np_error_code create_socket_ipv4(struct np_udp_socket* s)
{
    int sock = nm_zephyr_net_udp_nonblocking_socket(AF_INET, SOCK_DGRAM);
    if (sock == -1) {
        return NABTO_EC_UDP_SOCKET_CREATION_ERROR;
    }
    s->type = NABTO_IPV4;
    s->sock = sock;
    return NABTO_EC_OK;
}

bool init_mdns_ipv6_socket(int sock)
{
    int reuse = 1;
    if (zsock_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        return false;
    }

    struct sockaddr_in6 si_me;
    memset(&si_me, 0, sizeof(si_me));
    si_me.sin6_family = AF_INET6;
    si_me.sin6_port = htons(5353);
    si_me.sin6_addr = in6addr_any;
    if (zsock_bind(sock, (struct sockaddr*)&si_me, sizeof(si_me)) < 0) {
        NABTO_LOG_INFO(LOG, "bind mdns ipv6 failed (%d) %s", errno, strerror(errno));
        return false;
    }

    return true;
}


bool init_mdns_ipv4_socket(int sock)
{
    int reuse = 1;
    if (zsock_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        NABTO_LOG_ERROR(LOG, "could not set SO_REUSEADDR");
        return false;
    }

    struct sockaddr_in si_me;
    memset(&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(5353);
    si_me.sin_addr.s_addr = INADDR_ANY;
    if (zsock_bind(sock, (struct sockaddr*)&si_me, sizeof(si_me)) < 0) {
        NABTO_LOG_ERROR(LOG, "bind failed");
        return false;
    }

    return true;
}

void mdns_update_ipv4_socket_registration(int sock)
{
    struct in_addr addr;

    addr.s_addr = htonl(0xE00000FB); // 224.0.0.251
    struct net_if *iface;
    iface = net_if_get_default();
    int ec = net_ipv4_igmp_join(iface, &addr);
    if (ec != 0) {
        NABTO_LOG_ERROR(LOG, "Could not join ipv4 mdns multicast group.");
    }
}

void mdns_update_ipv6_socket_registration(int sock)
{
    struct net_if *iface;
    iface = net_if_get_default();
    struct in6_addr addr;
    net_ipv6_addr_create(&addr, 0xff02, 0, 0, 0, 0, 0, 0, 0x00fb);
    struct net_if_mcast_addr* maddr =  net_if_ipv6_maddr_add(iface, &addr);
    if (maddr == NULL) {
        NABTO_LOG_ERROR(LOG, "Could not add ipv6 multicast address.");
    } else {
        net_if_ipv6_maddr_join(maddr);
    }
}
