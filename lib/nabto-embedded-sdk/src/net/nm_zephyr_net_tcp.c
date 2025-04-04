#include "nm_zephyr_net_tcp.h"

#include <platform/np_util.h>
#include <platform/np_logging.h>
#include <platform/np_completion_event.h>
#include <platform/np_allocator.h>

#include <fcntl.h>
#include <zephyr/net/socket.h>

#define LOG NABTO_LOG_MODULE_TCP

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static np_error_code create(struct np_tcp* obj, struct np_tcp_socket** sock);
static void destroy(struct np_tcp_socket* sock);
static void async_connect(struct np_tcp_socket* sock, struct np_ip_address* address, uint16_t port, struct np_completion_event* completionEvent);
static void async_write(struct np_tcp_socket* sock, const void* data, size_t dataLength, struct np_completion_event* completionEvent);
static void async_read(struct np_tcp_socket* sock, void* buffer, size_t bufferLength, size_t* readLength, struct np_completion_event* completionEvent);
static void tcp_shutdown(struct np_tcp_socket* sock);
static void tcp_abort(struct np_tcp_socket* sock);

static void is_connected(struct np_tcp_socket* sock);
static void tcp_do_write(struct np_tcp_socket* sock);
static void tcp_do_read(struct np_tcp_socket* sock);

static struct np_tcp_functions module = {
    .create = &create,
    .destroy = &destroy,
    .async_connect = &async_connect,
    .async_write = &async_write,
    .async_read = &async_read,
    .shutdown = &tcp_shutdown,
    .abort = &tcp_abort
};

struct np_tcp nm_zephyr_net_tcp_get_impl(struct nm_zephyr_net* ctx)
{
    struct np_tcp obj;
    obj.mptr = &module;
    obj.data = ctx;
    return obj;
}

void nm_zephyr_net_tcp_free_socket(struct np_tcp_socket* sock)
{
    nm_zephyr_net_lock(sock->selectCtx);
    nn_llist_erase_node(&sock->tcpSocketsNode);
    nm_zephyr_net_unlock(sock->selectCtx);

    tcp_abort(sock);
    zsock_shutdown(sock->fd, ZSOCK_SHUT_RDWR);

    nm_zephyr_net_lock(sock->selectCtx);
    // add the socket to a list for pending close such that we do not close a socket while it is being polled on.
    nn_llist_append(&sock->selectCtx->tcpSocketsPendingClose, &sock->tcpSocketsNode, sock);
    nm_zephyr_net_unlock(sock->selectCtx);
}

void nm_zephyr_net_tcp_socket_free_from_poll_loop(struct np_tcp_socket* tcpSocket)
{
    NABTO_LOG_INFO(LOG, "nm_zephyr_net_tcp_socket_free_from_poll_loop");
    zsock_close(tcpSocket->fd);
    np_free(tcpSocket);
}


bool nm_zephyr_net_tcp_add_to_poll(struct np_tcp_socket* s, struct zsock_pollfd* pollfd)
{
    if (s->fd == -1) {
        return false;
    }

    pollfd->events = 0;

    bool status = false;
    if (s->read.completionEvent != NULL) {
        pollfd->fd = s->fd;
        pollfd->events |= ZSOCK_POLLIN;
        status = true;
    }
    if (s->write.completionEvent != NULL || s->connect.completionEvent != NULL) {
        pollfd->fd = s->fd;
        pollfd->events |= ZSOCK_POLLOUT;
        status = true;
    }

    return status;
}

void handle_poll_error(struct np_tcp_socket* s, np_error_code ec)
{
    if (s->connect.completionEvent) {
        struct np_completion_event* ev = s->connect.completionEvent;
        s->connect.completionEvent = NULL;
        np_completion_event_resolve(ev, ec);
    }
    if (s->write.completionEvent) {
        struct np_completion_event* ev = s->write.completionEvent;
        s->write.completionEvent = NULL;
        np_completion_event_resolve(ev, ec);
    }
    if (s->read.completionEvent) {
        struct np_completion_event* ev = s->read.completionEvent;
        s->read.completionEvent = NULL;
        np_completion_event_resolve(ev, ec);
    }
}

void nm_zephyr_net_tcp_handle_poll_events(struct np_tcp_socket* s, struct zsock_pollfd* pollfd)
{
    if ((pollfd->revents & ZSOCK_POLLNVAL) != 0) {
        NABTO_LOG_ERROR(LOG, "The socket is invalid. %d", s->fd);
        handle_poll_error(s, NABTO_EC_INVALID_SOCKET);
    } else if ((pollfd->revents & ZSOCK_POLLHUP) != 0) {
        NABTO_LOG_ERROR(LOG, "The socket has receivec POLLHUP. %d", s->fd);
        // this is handled in the subsequent read(s).
    } else if ((pollfd->revents & ZSOCK_POLLERR) != 0) {
        NABTO_LOG_ERROR(LOG, "The socket has receivec POLLERR. %d", s->fd);
        handle_poll_error(s, NABTO_EC_UNKNOWN);
    }

    bool read = (pollfd->revents & ZSOCK_POLLIN) != 0;
    bool write = (pollfd->revents & ZSOCK_POLLOUT) != 0;

    if (read) {
        tcp_do_read(s);
    }
    if (write) {
        if (s->connect.completionEvent) {
            is_connected(s);
        }
        if (s->write.completionEvent) {
            tcp_do_write(s);
        }
    }
}

np_error_code create(struct np_tcp* obj, struct np_tcp_socket** sock)
{
    struct nm_zephyr_net* selectCtx = obj->data;
    struct np_tcp_socket* s = np_calloc(1,sizeof(struct np_tcp_socket));
    if (s == NULL) {
        return NABTO_EC_OUT_OF_MEMORY;
    }
    s->fd = -1;
    *sock = s;
    s->selectCtx = selectCtx;
    nm_zephyr_net_lock(selectCtx);
    nn_llist_append(&selectCtx->tcpSockets, &s->tcpSocketsNode, s);
    nm_zephyr_net_unlock(selectCtx);
    s->aborted = false;
    return NABTO_EC_OK;
}

void destroy(struct np_tcp_socket* sock)
{
    if (sock == NULL) {
        return;
    }
    nm_zephyr_net_tcp_free_socket(sock);
    return;
}

np_error_code async_connect_ec(struct np_tcp_socket* sock, struct np_ip_address* address, uint16_t port)
{
    int s;

    int type = SOCK_STREAM;
#ifdef SOCK_NONBLOCK
    // Linux
    type |= SOCK_NONBLOCK;
#endif

    if (address->type == NABTO_IPV4) {
        s = zsock_socket(AF_INET, type, 0);
        NABTO_LOG_INFO(LOG, "Trying to make v4 tcp socket status %d");
    } else if (address->type == NABTO_IPV6) {
        NABTO_LOG_INFO(LOG, "Trying to make v6 tcp socket");
        s = zsock_socket(AF_INET6, type, 0);
    } else {
        return NABTO_EC_NOT_SUPPORTED;
    }
    if (s < 0) {
        return NABTO_EC_UNKNOWN;
    }

    sock->fd = s;

    int flags;
#ifndef SOCK_NONBLOCK
    // Mac
    flags = zsock_fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) {
        NABTO_LOG_ERROR(LOG, "cannot set nonblocking mode, fcntl F_GETFL failed");
        return NABTO_EC_UNKNOWN;
    }
    if (zsock_fcntl(sock->fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        NABTO_LOG_ERROR(LOG, "cannot set nonblocking mode, fcntl F_SETFL failed");
        return NABTO_EC_UNKNOWN;
    }
#endif


    flags = 1;
    if (zsock_setsockopt(sock->fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flags, sizeof(int)) != 0) {
        NABTO_LOG_ERROR(LOG, "Could not set socket option TCP_NODELAY");
    }

#if defined(SOL_SOCKET) && defined(SO_NOSIGPIPE)
    // Mac
    flags = 1;
    if (zsock_setsockopt(sock->fd, SOL_SOCKET, SO_NOSIGPIPE, (char *) &flags, sizeof(int)) != 0) {
        NABTO_LOG_ERROR(LOG, "Could not set socket option SO_NOSIGPIPE");
    }
#endif

    {
        int status;
        if (address->type == NABTO_IPV4) {
            struct sockaddr_in host;

            memset(&host,0,sizeof(struct sockaddr_in));
            host.sin_family = AF_INET;
            memcpy((void*)&host.sin_addr, address->ip.v4, 4);
            host.sin_port = htons(port);
            status = zsock_connect(sock->fd, (struct sockaddr*)&host, sizeof(struct sockaddr_in));
        } else { // Must be ipv6 (address->type == NABTO_IPV6) {
            struct sockaddr_in6 host;

            memset(&host,0,sizeof(struct sockaddr_in6));
            host.sin6_family = AF_INET6;
            memcpy(host.sin6_addr.s6_addr, address->ip.v6, 16);
            host.sin6_port = htons(port);
            status = zsock_connect(sock->fd, (struct sockaddr*)&host, sizeof(struct sockaddr_in6));
        }
        if (status == 0) {
            // connected
            return NABTO_EC_OK;
        } else {
            if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
                // OK
            } else {
                NABTO_LOG_ERROR(LOG, "Connect failed %s", strerror(errno));
                return NABTO_EC_UNKNOWN;
            }
            nm_zephyr_net_notify(sock->selectCtx);
        }
    }
    return NABTO_EC_AGAIN;
}

void async_connect(struct np_tcp_socket* sock, struct np_ip_address* address, uint16_t port, struct np_completion_event* completionEvent)
{
    if (sock->aborted) {
        np_completion_event_resolve(completionEvent, NABTO_EC_ABORTED);
        return;
    }
    if (sock->connect.completionEvent) {
        np_completion_event_resolve(completionEvent, NABTO_EC_OPERATION_IN_PROGRESS);
        return;
    }


    sock->connect.completionEvent = completionEvent;
    np_error_code ec = async_connect_ec(sock, address, port);
    if (ec == NABTO_EC_AGAIN) {
        // a deferred operation has begun
        return;
    } else {
        // connected or error.
        sock->connect.completionEvent = NULL;
        np_completion_event_resolve(completionEvent, ec);
        return;
    }
}

np_error_code is_connected_ec(struct np_tcp_socket* sock)
{
    int err;
    socklen_t len;
    len = sizeof(err);
    if (zsock_getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
        NABTO_LOG_ERROR(LOG, "getsockopt error %s",strerror(errno));
        return NABTO_EC_UNKNOWN;
    } else {
        if (err == 0) {
            return NABTO_EC_OK;
        } else if ( err == EINPROGRESS) {
            // Wait for next event
            return NABTO_EC_AGAIN;
        } else {
            NABTO_LOG_ERROR(LOG, "Cannot connect socket %s", strerror(err));
            return NABTO_EC_UNKNOWN;
        }
    }
}

void is_connected(struct np_tcp_socket* sock) {
    NABTO_LOG_INFO(LOG, "is_connected");
    if (sock->connect.completionEvent == NULL) {
        return;
    }

    np_error_code ec = is_connected_ec(sock);
    if (ec != NABTO_EC_AGAIN) {
        struct np_completion_event* ev = sock->connect.completionEvent;
        sock->connect.completionEvent = NULL;
        np_completion_event_resolve(ev, ec);
    }
}

np_error_code tcp_do_write_ec(struct np_tcp_socket* sock)
{
    NABTO_LOG_INFO(LOG, "tcp_do_write_ec");
    int sent = zsock_send(sock->fd, sock->write.data, sock->write.dataLength, MSG_NOSIGNAL);
    if (sent < 0) {
        if (sent == EAGAIN || sent == EWOULDBLOCK) {
            // Wait for next event which triggers write.
            return NABTO_EC_AGAIN;
        } else {
            return NABTO_EC_UNKNOWN;
        }
    } else {
        sock->write.data = (uint8_t*)sock->write.data + sent;
        sock->write.dataLength -= sent;
        if (sock->write.dataLength > 0) {
            // Wait for next event which triggers write.
            return NABTO_EC_AGAIN;
        } else {
            return NABTO_EC_OK;
        }
    }
}

void tcp_do_write(struct np_tcp_socket* sock)
{
    if (sock->write.completionEvent == NULL) {
        // nothing to write
        return;
    }

    np_error_code ec = tcp_do_write_ec(sock);
    if (ec != NABTO_EC_AGAIN) {
        struct np_completion_event* ev = sock->write.completionEvent;
        sock->write.completionEvent = NULL;
        np_completion_event_resolve(ev, ec);
    }
}

np_error_code async_write_ec(struct np_tcp_socket* sock, const void* data, size_t dataLength)

{
    NABTO_LOG_INFO(LOG, "async_write_ec");
    sock->write.data = data;
    sock->write.dataLength = dataLength;

    nm_zephyr_net_notify(sock->selectCtx);
    return NABTO_EC_AGAIN;
}

void async_write(struct np_tcp_socket* sock, const void* data, size_t dataLength, struct np_completion_event* completionEvent)
{
    NABTO_LOG_INFO(LOG, "async_write");
    if (sock->aborted) {
        np_completion_event_resolve(completionEvent, NABTO_EC_ABORTED);
        return;
    }
    if (sock->write.completionEvent != NULL) {
        np_completion_event_resolve(completionEvent, NABTO_EC_OPERATION_IN_PROGRESS);
        return;
    }

    sock->write.completionEvent = completionEvent;

    np_error_code ec = async_write_ec(sock, data, dataLength);
    if (ec != NABTO_EC_AGAIN) {
        sock->write.completionEvent = NULL;
        np_completion_event_resolve(completionEvent, ec);
    }
}

np_error_code tcp_do_read_ec(struct np_tcp_socket* sock)
{
    NABTO_LOG_INFO(LOG, "tcp_do_read_ec");
    int readen = zsock_recv(sock->fd, sock->read.buffer, sock->read.bufferSize, 0);
    if (readen == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return NABTO_EC_AGAIN;
        } else {
            NABTO_LOG_ERROR(LOG, "recv error %s", strerror(errno));
            return NABTO_EC_UNKNOWN;
        }
    } else if (readen == 0) {
        return NABTO_EC_EOF;
    } else {
        *(sock->read.readLength) = readen;
        return NABTO_EC_OK;
    }
}

void tcp_do_read(struct np_tcp_socket* sock)
{
    NABTO_LOG_INFO(LOG, "tcp_do_read");
    if (sock->read.completionEvent == NULL) {
        return;
    }

    np_error_code ec = tcp_do_read_ec(sock);

    if (ec != NABTO_EC_AGAIN) {
        struct np_completion_event* ev = sock->read.completionEvent;
        sock->read.completionEvent = NULL;
        np_completion_event_resolve(ev, ec);
    }
}


void async_read(struct np_tcp_socket* sock, void* buffer, size_t bufferSize, size_t* readLength, struct np_completion_event* completionEvent)
{
    NABTO_LOG_INFO(LOG, "async_read");
    if (sock->aborted) {
        np_completion_event_resolve(completionEvent, NABTO_EC_ABORTED);
        return;
    }
    if (sock->read.completionEvent != NULL) {
        np_completion_event_resolve(completionEvent, NABTO_EC_OPERATION_IN_PROGRESS);
        return;
    }
    sock->read.buffer = buffer;
    sock->read.bufferSize = bufferSize;
    sock->read.readLength = readLength;
    sock->read.completionEvent = completionEvent;
    nm_zephyr_net_notify(sock->selectCtx);
}

void tcp_shutdown(struct np_tcp_socket* sock)
{
    NABTO_LOG_INFO(LOG, "tcp_shutdown");
    zsock_shutdown(sock->fd, ZSOCK_SHUT_WR);
}

void tcp_abort(struct np_tcp_socket* sock)
{
    NABTO_LOG_INFO(LOG, "tcp_abort");
    if (sock->aborted) {
        return;
    }
    sock->aborted = true;
    if (sock->read.completionEvent != NULL) {
        struct np_completion_event* ev = sock->read.completionEvent;
        sock->read.completionEvent = NULL;
        np_completion_event_resolve(ev, NABTO_EC_ABORTED);
    }
    if (sock->write.completionEvent != NULL) {
        struct np_completion_event* ev = sock->write.completionEvent;
        sock->write.completionEvent = NULL;
        np_completion_event_resolve(ev, NABTO_EC_ABORTED);
    }
    if (sock->connect.completionEvent) {
        struct np_completion_event* ev = sock->connect.completionEvent;
        sock->connect.completionEvent = NULL;
        np_completion_event_resolve(ev, NABTO_EC_ABORTED);
    }
}
