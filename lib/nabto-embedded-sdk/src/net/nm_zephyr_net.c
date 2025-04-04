#include "nm_zephyr_net.h"
#include "nm_zephyr_net_udp.h"
#include "nm_zephyr_net_tcp.h"
#include <nn/llist.h>

#include <platform/np_logging.h>
#include <platform/np_util.h>


#include <zephyr/net/socket.h>
#include <zephyr/net/socket_select.h>

#define LOG NABTO_LOG_MODULE_UDP

/**
 * Helper function declarations
 */
static void build_fd_sets(struct nm_zephyr_net* ctx);
static void network_thread(void* data, void* data2, void* data3);

static int nm_zephyr_net_inf_wait(struct nm_zephyr_net* ctx);

static void nm_zephyr_net_read(struct nm_zephyr_net* ctx, int nfds);

static void add_pipes(struct nm_zephyr_net* ctx);
static void add_tcp_sockets(struct nm_zephyr_net* ctx);
static void add_udp_sockets(struct nm_zephyr_net* ctx);

#define NM_ZEPHYR_NET_STACK_SIZE 2048
K_THREAD_STACK_DEFINE(stackData, NM_ZEPHYR_NET_STACK_SIZE);
struct k_thread threadData;


/**
 * On some platforms udp sockets can reach a state where they are no longer
 * useable. On these platforms zsock_poll returns an error for the affected
 * socket and the socket must be closed. To ensure continued operation of the
 * platform the socket is later recreated and recv and send operations continue
 * on the new socket. Ofcourse the new socket has changed port numbers etc. for
 * all
 *
 * Platforms: NRF9160
 */
static void socketRecreateTimerExpire(struct k_timer *timer);
static void socketRecreateWorkHandler(struct k_work *work);

/**
 * Api functions start
 */
np_error_code nm_zephyr_net_init(struct nm_zephyr_net* ctx)
{
    nn_llist_init(&ctx->udpSockets);
    nn_llist_init(&ctx->tcpSockets);
    nn_llist_init(&ctx->tcpSocketsPendingClose);
    k_mutex_init(&ctx->mutex);
    ctx->stopped = false;
    ctx->running = false;
    if (zsock_socketpair(AF_LOCAL, SOCK_STREAM, 0, ctx->pipefd) != 0) {
        NABTO_LOG_ERROR(LOG, "Failed to create pipe %s", errno);
        return NABTO_EC_UNKNOWN;
    }

    k_timer_init(&ctx->socketRecreateTimer, socketRecreateTimerExpire, NULL);
    k_timer_start(&ctx->socketRecreateTimer, K_SECONDS(10), K_SECONDS(10));
    k_work_init(&ctx->socketRecreateWork, socketRecreateWorkHandler);

    return NABTO_EC_OK;
}

void nm_zephyr_net_deinit(struct nm_zephyr_net* ctx)
{
    k_timer_stop(&ctx->socketRecreateTimer);
    nm_zephyr_net_stop(ctx);

    if (ctx->running) {
        k_thread_join(&threadData, K_FOREVER);
        ctx->running = false;
    }
    zsock_close(ctx->pipefd[0]);
    zsock_close(ctx->pipefd[1]);
}

void nm_zephyr_net_run(struct nm_zephyr_net* ctx)
{
    k_thread_create(&threadData, stackData, K_THREAD_STACK_SIZEOF(stackData), network_thread,
        ctx, NULL, NULL,
        2 /** TODO thread priority */, 0, K_NO_WAIT);
    ctx->running = true;
}

void nm_zephyr_net_stop(struct nm_zephyr_net* ctx)
{
    nm_zephyr_net_lock(ctx);
    ctx->stopped = true;
    nm_zephyr_net_unlock(ctx);
    nm_zephyr_net_notify(ctx);
}

int nm_zephyr_net_inf_wait(struct nm_zephyr_net* ctx)
{
    int nfds;
    build_fd_sets(ctx);

    nfds = zsock_poll(ctx->pollfds, ctx->usedPollfds, INT_MAX);
    if (nfds < 0) {
        NABTO_LOG_ERROR(LOG, "Error in zsock_poll: (%i), errno (%s)", nfds, strerror(errno));
    } else {
        NABTO_LOG_TRACE(LOG, "zsock_poll returned with %i file descriptors", nfds);
    }
    return nfds;
}

int nm_zephyr_net_timed_wait(struct nm_zephyr_net* ctx, uint32_t ms)
{
    NABTO_LOG_TRACE(LOG, "timed wait %d", ms);
    int nfds;
    build_fd_sets(ctx);
    nfds = zsock_poll(ctx->pollfds, ctx->usedPollfds, ms);
    if (nfds < 0) {
        NABTO_LOG_ERROR(LOG, "Error in select wait: (%i) '%s'", errno, strerror(errno));
    }
    return nfds;
}

static void nm_zephyr_net_read(struct nm_zephyr_net* ctx, int nfds)
{
    for (size_t i = 0; i < ctx->usedPollfds; i++) {
        struct zsock_pollfd* pollfd = &ctx->pollfds[i];
        struct pollfd_info* info = &ctx->pollfdInfo[i];
        if ((pollfd->revents & (ZSOCK_POLLIN | ZSOCK_POLLOUT | ZSOCK_POLLERR | ZSOCK_POLLHUP | ZSOCK_POLLNVAL)) != 0) {
            if (info->type == NM_ZEPHYR_NET_TYPE_PIPE) {
                char one;
                (void)zsock_recv(pollfd->fd, &one, 1, 0);
            } else if (info->type == NM_ZEPHYR_NET_TYPE_UDP) {
                struct np_udp_socket* s = info->ptr;
                nm_zephyr_net_udp_handle_poll_events(s, pollfd);
            } else if (info->type == NM_ZEPHYR_NET_TYPE_TCP) {
                struct np_tcp_socket* s = info->ptr;
                nm_zephyr_net_tcp_handle_poll_events(s, pollfd);
            }
        }
    }
}

/**
 * Helper functions start
 */

void build_fd_sets(struct nm_zephyr_net* ctx)
{
    memset(ctx->pollfds, 0, sizeof(struct zsock_pollfd) * CONFIG_NET_SOCKETS_POLL_MAX);
    memset(ctx->pollfdInfo, 0, sizeof(struct pollfd_info) * CONFIG_NET_SOCKETS_POLL_MAX);

    nm_zephyr_net_lock(ctx);
    add_pipes(ctx);
    add_udp_sockets(ctx);
    add_tcp_sockets(ctx);
    nm_zephyr_net_unlock(ctx);
}

void add_pipes(struct nm_zephyr_net* ctx)
{
    ctx->pollfds[0].fd = ctx->pipefd[0];
    ctx->pollfds[1].fd = ctx->pipefd[1];
    ctx->pollfds[0].events = ZSOCK_POLLIN;
    ctx->pollfds[1].events = ZSOCK_POLLIN;
    ctx->pollfdInfo[0].type = NM_ZEPHYR_NET_TYPE_PIPE;
    ctx->pollfdInfo[1].type = NM_ZEPHYR_NET_TYPE_PIPE;
    ctx->usedPollfds = 2;
}

void add_udp_sockets(struct nm_zephyr_net* ctx)
{
    struct np_udp_socket* s;
    NN_LLIST_FOREACH(s, &ctx->udpSockets)
    {
        if (s->state != NM_ZEPHYR_UDP_SOCKET_STATE_BOUND) {
            continue;
        }

        if (ctx->usedPollfds > CONFIG_NET_SOCKETS_POLL_MAX-1) {
            NABTO_LOG_ERROR(LOG, "CONFIG_NET_SOCKETS_POLL_MAX set too low");
            return;
        }
        struct zsock_pollfd* pollfd = &ctx->pollfds[ctx->usedPollfds];
        if(nm_zephyr_net_udp_add_to_poll(s, pollfd)) {
            ctx->pollfdInfo[ctx->usedPollfds].type = NM_ZEPHYR_NET_TYPE_UDP;
            ctx->pollfdInfo[ctx->usedPollfds].ptr = s;
            ctx->usedPollfds++;
        }
    }
}

static void add_tcp_sockets(struct nm_zephyr_net* ctx)
{
    struct np_tcp_socket* s;
    NN_LLIST_FOREACH(s, &ctx->tcpSockets)
    {
        if (ctx->usedPollfds > CONFIG_NET_SOCKETS_POLL_MAX-1) {
            NABTO_LOG_ERROR(LOG, "CONFIG_NET_SOCKETS_POLL_MAX set too low");
            return;
        }
        struct zsock_pollfd* pollfd = &ctx->pollfds[ctx->usedPollfds];
        if(nm_zephyr_net_tcp_add_to_poll(s, pollfd)) {
            ctx->pollfdInfo[ctx->usedPollfds].type = NM_ZEPHYR_NET_TYPE_TCP;
            ctx->pollfdInfo[ctx->usedPollfds].ptr = s;
            ctx->usedPollfds++;
        }
    }
}

static void nm_zephyr_net_remove_tcp_sockets(struct nm_zephyr_net* ctx)
{
    struct nn_llist_iterator it = nn_llist_begin(&ctx->tcpSocketsPendingClose);
    while ( !nn_llist_is_end(&it) ) {
        struct np_tcp_socket* tcpSocket = nn_llist_get_item(&it);
        struct nn_llist_iterator current = it;
        nn_llist_next(&it);
        nm_zephyr_net_tcp_socket_free_from_poll_loop(tcpSocket);
        nn_llist_erase(&current);
    }
}

void nm_zephyr_net_notify(struct nm_zephyr_net* ctx)
{
    (void)zsock_send(ctx->pipefd[1], "1", 1, 0);
}

np_error_code nm_zephyr_net_down(struct nm_zephyr_net* ctx)
{
    np_error_code retEc = NABTO_EC_OK;
    nm_zephyr_net_lock(ctx);
    struct np_udp_socket* s;
    NN_LLIST_FOREACH(s, &ctx->udpSockets)
    {
        np_error_code ec = nm_zephyr_net_udp_down(s);
        if (ec != NABTO_EC_OK) {
            NABTO_LOG_ERROR(LOG, "Cannot bring the socket down");
            retEc = ec;
        }
    }
    nm_zephyr_net_unlock(ctx);
    return retEc;
}

np_error_code nm_zephyr_net_up(struct nm_zephyr_net* ctx)
{
    NABTO_LOG_TRACE(LOG, "nm_zephyr_net_up called");
    np_error_code retEc = NABTO_EC_OK;
    nm_zephyr_net_lock(ctx);
    struct np_udp_socket* s;
    NN_LLIST_FOREACH(s, &ctx->udpSockets)
    {
        np_error_code ec = nm_zephyr_net_udp_up(ctx, s);
        if (ec != NABTO_EC_OK) {
            NABTO_LOG_ERROR(LOG, "Cannot bring the socket up");
            retEc = ec;
        }
    }
    nm_zephyr_net_unlock(ctx);
    return retEc;
}

void network_thread(void* data, void* data2, void* data3)
{
    k_thread_name_set(NULL, "network_thread");

    struct nm_zephyr_net* ctx = data;
    while(true) {
        int nfds;
        nm_zephyr_net_lock(ctx);
        bool stopped = ctx->stopped;
        nm_zephyr_net_unlock(ctx);
        nm_zephyr_net_remove_tcp_sockets(ctx);
        if (stopped) {
            return;
        } else {
            // Wait for events.
            nfds = nm_zephyr_net_inf_wait(ctx);
            if (nfds > 0) {
                nm_zephyr_net_read(ctx, nfds);
            } else if (nfds < 0) {
                k_sleep(K_MSEC(1000));
            }

        }
    }
}

static void socketRecreateTimerExpire(struct k_timer *timer)
{
    struct nm_zephyr_net *net =
		CONTAINER_OF(timer, struct nm_zephyr_net, socketRecreateTimer);
    k_work_submit(&net->socketRecreateWork);
}

static void socketRecreateWorkHandler(struct k_work *work)
{
    struct nm_zephyr_net *net =
		CONTAINER_OF(work, struct nm_zephyr_net, socketRecreateWork);
    nm_zephyr_net_up(net);
}

void nm_zephyr_net_lock(struct nm_zephyr_net* ctx)
{
    k_mutex_lock(&ctx->mutex, K_FOREVER);
}

void nm_zephyr_net_unlock(struct nm_zephyr_net* ctx)
{
    k_mutex_unlock(&ctx->mutex);
}
