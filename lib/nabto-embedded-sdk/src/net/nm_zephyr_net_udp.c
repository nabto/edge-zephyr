#include "nm_zephyr_net_udp.h"

#include <platform/np_logging.h>
#include <platform/np_util.h>
#include <platform/np_completion_event.h>
#include <platform/np_allocator.h>

#include <zephyr/net/socket.h>

// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <errno.h>
// #include <string.h>
// #include <unistd.h>
#include <fcntl.h>
// #include <arpa/inet.h>
// #include <net/if.h>
// #include <ifaddrs.h>

#define LOG NABTO_LOG_MODULE_UDP

/**
 * Helper function declarations
 */
static void nm_zephyr_net_udp_handle_event(struct np_udp_socket* sock, np_error_code ec);
static void nm_zephyr_net_udp_free_socket(struct np_udp_socket* sock);

/**
 * Api function declarations
 */
static np_error_code nm_zephyr_net_udp_create(struct np_udp* obj, struct np_udp_socket** sock);
static void nm_zephyr_net_udp_destroy(struct np_udp_socket* sock);
static void nm_zephyr_net_udp_abort(struct np_udp_socket* sock);
static void nm_zephyr_net_udp_async_bind_port(struct np_udp_socket* sock, uint16_t port, struct np_completion_event* completionEvent);
static void nm_zephyr_net_udp_async_send_to(struct np_udp_socket* sock, struct np_udp_endpoint* ep,
                                             uint8_t* buffer, uint16_t bufferSize,
                                             struct np_completion_event* completionEvent);
static void nm_zephyr_net_udp_async_recv_wait(struct np_udp_socket* socket, struct np_completion_event* completionEvent);
static np_error_code nm_zephyr_net_udp_recv_from(struct np_udp_socket* socket, struct np_udp_endpoint* ep, uint8_t* buffer, size_t bufferSize, size_t* readLength);
static uint16_t nm_zephyr_net_udp_get_local_port(struct np_udp_socket* socket);

static np_error_code udp_send_to(struct np_udp_socket* s, const struct np_udp_endpoint* ep, const uint8_t* buffer, uint16_t bufferSize);
static np_error_code udp_recv_from(struct np_udp_socket* sock, struct np_udp_endpoint* ep, uint8_t* buffer, size_t bufferSize, size_t* readLength);
static np_error_code bind_port(struct np_udp_socket* s, uint16_t port);
static uint16_t get_local_port(struct np_udp_socket* s);
static np_error_code create_socket_any(struct np_udp_socket *s);
np_error_code nm_zephyr_net_udp_async_bind_port_ec(struct np_udp_socket *sock, uint16_t port);

static struct np_udp_functions module = {
    .create           = &nm_zephyr_net_udp_create,
    .destroy          = &nm_zephyr_net_udp_destroy,
    .abort            = &nm_zephyr_net_udp_abort,
    .async_bind_port  = &nm_zephyr_net_udp_async_bind_port,
    .async_send_to    = &nm_zephyr_net_udp_async_send_to,
    .async_recv_wait  = &nm_zephyr_net_udp_async_recv_wait,
    .recv_from        = &nm_zephyr_net_udp_recv_from,
    .get_local_port   = &nm_zephyr_net_udp_get_local_port
};

struct np_udp nm_zephyr_net_udp_get_impl(struct nm_zephyr_net* ctx)
{
    struct np_udp obj;
    obj.mptr = &module;
    obj.data = ctx;
    return obj;
}

np_error_code nm_zephyr_net_udp_down(struct np_udp_socket *udpSocket)
{
    if (udpSocket->state == NM_ZEPHYR_UDP_SOCKET_STATE_BOUND) {
        zsock_close(udpSocket->sock);
        udpSocket->state = NM_ZEPHYR_UDP_SOCKET_STATE_DOWN;
    }
    return NABTO_EC_OK;
}
np_error_code nm_zephyr_net_udp_up(struct nm_zephyr_net* netCtx, struct np_udp_socket *udpSocket)
{
    np_error_code ec = NABTO_EC_OK;
    if (udpSocket->state == NM_ZEPHYR_UDP_SOCKET_STATE_DOWN)
    {
        ec = nm_zephyr_net_udp_async_bind_port_ec(udpSocket, udpSocket->port);
        if (ec == NABTO_EC_OK) {
            nm_zephyr_net_notify(netCtx);
        }
    }
    return NABTO_EC_OK;
}


np_error_code nm_zephyr_net_udp_create(struct np_udp* obj, struct np_udp_socket** sock)
{
    struct np_udp_socket* s = np_calloc(1, sizeof(struct np_udp_socket));
    if (!s) {
        return NABTO_EC_OUT_OF_MEMORY;
    }
    *sock = s;
    s->sock = -1;

    struct nm_zephyr_net* selectCtx = obj->data;

    s->selectCtx = selectCtx;
    s->state = NM_ZEPHYR_UDP_SOCKET_STATE_NEW;

    nm_zephyr_net_lock(selectCtx);
    nn_llist_append(&selectCtx->udpSockets, &s->udpSocketsNode, s);
    nm_zephyr_net_unlock(selectCtx);

    // add fd to select fd set.
    nm_zephyr_net_notify(selectCtx);

    return NABTO_EC_OK;
}

np_error_code nm_zephyr_net_udp_async_bind_port_ec(struct np_udp_socket* sock, uint16_t port)
{
    if (sock->state == NM_ZEPHYR_UDP_SOCKET_STATE_ABORTED) {
        NABTO_LOG_ERROR(LOG, "bind called on aborted socket");
        return NABTO_EC_ABORTED;
    }

    np_error_code ec;

    ec = create_socket_any(sock);
    if (ec != NABTO_EC_OK) {
        return ec;
    }

    ec = bind_port(sock, port);
    if (ec != NABTO_EC_OK) {
        zsock_close(sock->sock);
    }

    return NABTO_EC_OK;
}

void nm_zephyr_net_udp_async_bind_port(struct np_udp_socket* sock, uint16_t port, struct np_completion_event* completionEvent)
{
    np_error_code ec = nm_zephyr_net_udp_async_bind_port_ec(sock, port);
    np_completion_event_resolve(completionEvent, ec);
}


np_error_code nm_zephyr_net_udp_async_send_to_ec(struct np_udp_socket* sock, struct np_udp_endpoint* ep,
                                                  uint8_t* buffer, uint16_t bufferSize)
{
    if (sock->state == NM_ZEPHYR_UDP_SOCKET_STATE_ABORTED) {
        NABTO_LOG_ERROR(LOG, "send to called on aborted socket");
        return NABTO_EC_ABORTED;
    }

    if (sock->state != NM_ZEPHYR_UDP_SOCKET_STATE_BOUND)
    {
        NABTO_LOG_INFO(LOG, "dropping packet due to unbound socket");
        // We have no socket, just drop the packet
        return NABTO_EC_OK;
    }

    np_error_code ec = udp_send_to(sock, ep, buffer, bufferSize);
    if (ec != NABTO_EC_OK) {
        return ec;
    }
    return NABTO_EC_OK;
}

void nm_zephyr_net_udp_async_send_to(struct np_udp_socket* sock, struct np_udp_endpoint* ep,
                                      uint8_t* buffer, uint16_t bufferSize,
                                      struct np_completion_event* completionEvent)
{
    np_error_code ec = nm_zephyr_net_udp_async_send_to_ec(sock, ep, buffer, bufferSize);
    np_completion_event_resolve(completionEvent, ec);
}


void nm_zephyr_net_udp_async_recv_wait(struct np_udp_socket* sock,
                                        struct np_completion_event* completionEvent)
{
    if (sock->state == NM_ZEPHYR_UDP_SOCKET_STATE_ABORTED) {
        NABTO_LOG_ERROR(LOG, "recv from called on aborted socket");
        np_completion_event_resolve(completionEvent, NABTO_EC_ABORTED);
        return;
    }

    if (sock->recv.completionEvent != NULL) {
        NABTO_LOG_ERROR(LOG, "operation already in progress");
        np_completion_event_resolve(completionEvent, NABTO_EC_OPERATION_IN_PROGRESS);
        return;
    }

    sock->recv.completionEvent = completionEvent;
    nm_zephyr_net_notify(sock->selectCtx);
    return;
}

np_error_code nm_zephyr_net_udp_recv_from(struct np_udp_socket* sock, struct np_udp_endpoint* ep, uint8_t* buffer, size_t bufferSize, size_t* readLength)
{
    if (sock->state == NM_ZEPHYR_UDP_SOCKET_STATE_DOWN) {
        return NABTO_EC_AGAIN;
    }
    return udp_recv_from(sock, ep, buffer, bufferSize, readLength);
}

uint16_t nm_zephyr_net_udp_get_local_port(struct np_udp_socket* sock)
{
    if (sock->state == NM_ZEPHYR_UDP_SOCKET_STATE_ABORTED) {
        NABTO_LOG_ERROR(LOG, "get local port called on aborted socket");
        return 0;
    }
    return get_local_port(sock);
}

void nm_zephyr_net_udp_abort(struct np_udp_socket* sock)
{
    if (sock->state != NM_ZEPHYR_UDP_SOCKET_STATE_ABORTED) {
        sock->state = NM_ZEPHYR_UDP_SOCKET_STATE_ABORTED;
    }

    if (sock->recv.completionEvent != NULL) {
        struct np_completion_event* ev = sock->recv.completionEvent;
        sock->recv.completionEvent = NULL;
        np_completion_event_resolve(ev, NABTO_EC_ABORTED);
    }
}

void nm_zephyr_net_udp_destroy(struct np_udp_socket* sock)
{
    if (sock == NULL) {
        return;
    }
    nm_zephyr_net_udp_free_socket(sock);
    return;
}

void nm_zephyr_net_udp_handle_event(struct np_udp_socket* sock, np_error_code ec)
{
    if (sock->recv.completionEvent != NULL) {
        struct np_completion_event* ev = sock->recv.completionEvent;
        sock->recv.completionEvent = NULL;
        np_completion_event_resolve(ev, ec);
    }
}

void nm_zephyr_net_udp_free_socket(struct np_udp_socket* sock)
{
    nm_zephyr_net_lock(sock->selectCtx);
    nn_llist_erase_node(&sock->udpSocketsNode);
    nm_zephyr_net_unlock(sock->selectCtx);

    nm_zephyr_net_udp_abort(sock);
    zsock_shutdown(sock->sock, ZSOCK_SHUT_RDWR);
    zsock_close(sock->sock);
    np_free(sock);
}


bool nm_zephyr_net_udp_add_to_poll(struct np_udp_socket* s, struct zsock_pollfd* pollfd)
{
    if (s->recv.completionEvent && s->sock != -1) {
        pollfd->fd = s->sock;
        pollfd->events = ZSOCK_POLLIN;
        return true;
    }
    return false;
}

/**
 * Return true iff the network is down.
 */
static bool is_netdown(int sock) {
    int err;
    socklen_t len;
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) == 0)
    {
        if (err == ENETDOWN)
        {
            return true;
        }
    } else {
        NABTO_LOG_ERROR(LOG, "Cannot get SO_ERROR from the socket");
    }
    return false;
}

void nm_zephyr_net_udp_handle_poll_events(struct np_udp_socket* s, struct zsock_pollfd* pollfd)
{
    if ((pollfd->revents & ZSOCK_POLLNVAL) != 0) {
        NABTO_LOG_ERROR(LOG, "The socket is invalid. %d", s->sock);
        nm_zephyr_net_udp_handle_event(s, NABTO_EC_UDP_SOCKET_ERROR);
    } else if ((pollfd->revents & ZSOCK_POLLHUP) != 0) {
        NABTO_LOG_ERROR(LOG, "The socket has receivec POLLHUP. %d", s->sock);
        nm_zephyr_net_udp_handle_event(s, NABTO_EC_UDP_SOCKET_ERROR);
    } else if ((pollfd->revents & (ZSOCK_POLLERR)) != 0)
    {
        if (is_netdown(s->sock)) {
            NABTO_LOG_INFO(LOG, "Socket goes to down state as the interface is brought down.");
            nm_zephyr_net_udp_down(s);
        } else {
            // TODO
            NABTO_LOG_ERROR(LOG, "An unhandled error occured on the socket %d", s->sock);
            nm_zephyr_net_udp_handle_event(s, NABTO_EC_UDP_SOCKET_ERROR);
        }
    } else if ((pollfd->revents & ZSOCK_POLLIN) != 0) {
        nm_zephyr_net_udp_handle_event(s, NABTO_EC_OK);
    }
}



int nm_zephyr_net_udp_nonblocking_socket(int domain, int type)
{
    int sock = zsock_socket(domain, type, 0);
    if (sock == -1) {
        NABTO_LOG_ERROR(LOG, "Cannot create socket domain: %d, type: %d, error: %s", domain, type, strerror(errno));
        return sock;
    }

    if (is_netdown(sock)) {
        NABTO_LOG_ERROR(LOG, "Cannot create socket as the network is down");
        zsock_close(sock);
        return -1;
    }

    int flags = zsock_fcntl(sock, F_GETFL, 0);
    if (flags == -1) flags = 0;
    zsock_fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    return sock;
}

np_error_code udp_send_to(struct np_udp_socket* s, const struct np_udp_endpoint* ep, const uint8_t* buffer, uint16_t bufferSize)
{
    ssize_t res;
    struct np_ip_address sendIp;

    if (s->type == ep->ip.type) {
        // No conversion needed.
        sendIp = ep->ip;
    } else if (s->type == NABTO_IPV6 && ep->ip.type == NABTO_IPV4) {
        // convert ipv4 to ipv6 mapped ipv4
        np_ip_convert_v4_to_v4_mapped(&ep->ip, &sendIp);
    } else if (s->type == NABTO_IPV4 && np_ip_is_v4_mapped(&ep->ip)) {
        np_ip_convert_v4_mapped_to_v4(&ep->ip, &sendIp);
    } else {
        NABTO_LOG_TRACE(LOG, "Cannot send ipv6 packets on an ipv4 socket.");
        return NABTO_EC_FAILED_TO_SEND_PACKET;
    }

    NABTO_LOG_TRACE(LOG, "Sending packet of size %d, to %s:%d on fd %d", bufferSize, np_ip_address_to_string(&sendIp), ep->port, s->sock);
    if (sendIp.type == NABTO_IPV4) {
        struct sockaddr_in srv_addr;
        srv_addr.sin_family = AF_INET;
        srv_addr.sin_port = htons (ep->port);
        memcpy((void*)&srv_addr.sin_addr, sendIp.ip.v4, sizeof(srv_addr.sin_addr));
        res = zsock_sendto (s->sock, buffer, bufferSize, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    } else { // IPv6
        struct sockaddr_in6 srv_addr;
        srv_addr.sin6_family = AF_INET6;
        //srv_addr.sin6_flowinfo = 0;
        srv_addr.sin6_scope_id = 0;
        srv_addr.sin6_port = htons (ep->port);
        memcpy((void*)&srv_addr.sin6_addr,sendIp.ip.v6, sizeof(srv_addr.sin6_addr));
        res = zsock_sendto (s->sock, buffer, bufferSize, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    }

    if (res < 0) {
        int status = errno;
        NABTO_LOG_TRACE(LOG, "UDP returned error status (%d) %s", status, strerror(status));
        if (status == EAGAIN || status == EWOULDBLOCK) {
            // expected
            // just drop the packet and the upper layers will take care of retransmissions.
        } else {

            if (status == EADDRNOTAVAIL || // if we send to ipv6 scopes we do not have
                status == ENETUNREACH || // if we send ipv6 on a system without it.
                status == EAFNOSUPPORT) // if we send ipv6 on an ipv4 only socket
            {
                NABTO_LOG_TRACE(LOG,"ERROR: (%i) '%s' in udp_send_to", (int) status, strerror(status));
            } else {
                NABTO_LOG_ERROR(LOG,"ERROR: (%i) '%s' in udp_send_to", (int) status, strerror(status));
            }
            return NABTO_EC_FAILED_TO_SEND_PACKET;
        }
    }
    NABTO_LOG_TRACE(LOG, "UDP sent packet with size (%d) on socket %d", bufferSize, s->sock);
    return NABTO_EC_OK;
}

np_error_code udp_recv_from(struct np_udp_socket* sock, struct np_udp_endpoint* ep, uint8_t* buffer, size_t bufferSize, size_t* readLength)
{
    ssize_t recvLength;
    if (sock->type == NABTO_IPV6) {
        struct sockaddr_in6 sa;
        socklen_t addrlen = sizeof(sa);
        recvLength = zsock_recvfrom(sock->sock, buffer, bufferSize, 0, (struct sockaddr*)&sa, &addrlen);
        memcpy(&ep->ip.ip.v6, &sa.sin6_addr.s6_addr, sizeof(ep->ip.ip.v6));
        ep->port = ntohs(sa.sin6_port);
        ep->ip.type = NABTO_IPV6;
    } else {
        struct sockaddr_in sa;
        socklen_t addrlen = sizeof(sa);
        recvLength = zsock_recvfrom(sock->sock, buffer, bufferSize, 0, (struct sockaddr*)&sa, &addrlen);
        memcpy(&ep->ip.ip.v4, &sa.sin_addr.s_addr, sizeof(ep->ip.ip.v4));
        ep->port = ntohs(sa.sin_port);
        ep->ip.type = NABTO_IPV4;
    }
    if (recvLength < 0) {
        int status = errno;
        if (status == EAGAIN || status == EWOULDBLOCK) {
            // expected
            // wait for next event to check for data.
            return NABTO_EC_AGAIN;
        } else {
            NABTO_LOG_ERROR(LOG,"ERROR: (%d) '%s' in udp_recv_from", status, strerror(status));
            return NABTO_EC_UDP_SOCKET_ERROR;
        }
    }

    *readLength = recvLength;
    return NABTO_EC_OK;
}

np_error_code bind_port(struct np_udp_socket* s, uint16_t port)
{
    int status;

    if (s->type == NABTO_IPV6) {
        struct sockaddr_in6 si_me6;
        memset(&si_me6, 0, sizeof(si_me6));
        si_me6.sin6_family = AF_INET6;
        si_me6.sin6_port = htons(port);
        si_me6.sin6_addr = in6addr_any;
        status = zsock_bind(s->sock, (struct sockaddr*)&si_me6, sizeof(si_me6));
    } else {
        struct sockaddr_in si_me;
        memset(&si_me, 0, sizeof(si_me));
        si_me.sin_family = AF_INET;
        si_me.sin_port = htons(port);
        si_me.sin_addr.s_addr = INADDR_ANY;
        status = zsock_bind(s->sock, (struct sockaddr*)&si_me, sizeof(si_me));
    }

    NABTO_LOG_TRACE(LOG, "bind returned %i", status);

    if (status == 0) {
        s->state = NM_ZEPHYR_UDP_SOCKET_STATE_BOUND;
        return NABTO_EC_OK;
    } else {
        return NABTO_EC_UDP_SOCKET_CREATION_ERROR;
    }
}

uint16_t get_local_port(struct np_udp_socket* s)
{
    if (s->type == NABTO_IPV6) {
        struct sockaddr_in6 addr;
        addr.sin6_port = 0;
        socklen_t length = sizeof(struct sockaddr_in6);
        zsock_getsockname(s->sock, (struct sockaddr*)(&addr), &length);
        return htons(addr.sin6_port);
    } else {
        struct sockaddr_in addr;
        addr.sin_port = 0;
        socklen_t length = sizeof(struct sockaddr_in);
        zsock_getsockname(s->sock, (struct sockaddr*)(&addr), &length);
        return htons(addr.sin_port);
    }
}

np_error_code create_socket_any(struct np_udp_socket* s)
{
    int sock;
    sock = nm_zephyr_net_udp_nonblocking_socket(AF_INET, SOCK_DGRAM);
    if (sock == -1) {
        return NABTO_EC_UDP_SOCKET_CREATION_ERROR;
    }

    s->type = NABTO_IPV4;
    s->sock = sock;
    return NABTO_EC_OK;
    // TODO when sending v6 mapped v4 addresses using send_to then the packet is not being sent.

    sock = nm_zephyr_net_udp_nonblocking_socket(AF_INET6, SOCK_DGRAM);
    if (sock == -1) {
        sock = nm_zephyr_net_udp_nonblocking_socket(AF_INET, SOCK_DGRAM);
        if (s->sock == -1) {
            int e = errno;
            NABTO_LOG_ERROR(LOG, "Unable to create socket: (%i) '%s'.", e, strerror(e));
            return NABTO_EC_UDP_SOCKET_CREATION_ERROR;
        } else {
            NABTO_LOG_WARN(LOG, "IPv4 socket opened since IPv6 socket creation failed");
            s->type = NABTO_IPV4;
        }
    } else {
        int no = 0;
        s->type = NABTO_IPV6;
        if (zsock_setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (void* ) &no, sizeof(no)))
        {
            int e = errno;
            NABTO_LOG_ERROR(LOG,"Unable to set option: (%i) '%s'.", e, strerror(e));

            zsock_close(s->sock);
            return NABTO_EC_UDP_SOCKET_CREATION_ERROR;
        }
    }
    s->sock = sock;
    return NABTO_EC_OK;
}
