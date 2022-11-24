#include "nm_zephyr_net.h"
#include "nm_zephyr_net_udp.h"
#include "nm_zephyr_net_tcp.h"

#include <platform/np_logging.h>
#include <platform/np_util.h>


#include <zephyr/net/socket.h>
#include <zephyr/net/socket_select.h>

#define LOG NABTO_LOG_MODULE_UDP

/**
 * Helper function declarations
 */
static void build_fd_sets();
static void network_thread(void* data, void* data2, void* data3);

static int nm_zephyr_net_inf_wait(struct nm_zephyr_net* ctx);

static void nm_zephyr_net_read(struct nm_zephyr_net* ctx, int nfds);


/**
 * Api functions start
 */
np_error_code nm_zephyr_net_init(struct nm_zephyr_net* ctx)
{
    nn_llist_init(&ctx->udpSockets);
    nn_llist_init(&ctx->tcpSockets);
    k_mutex_init(&ctx->mutex);
    ctx->stopped = false;
    ctx->running = false;
    if (zsock_socketpair(AF_LOCAL, SOCK_STREAM, 0, ctx->pipefd) != 0) {
        NABTO_LOG_ERROR(LOG, "Failed to create pipe %s", errno);
        return NABTO_EC_UNKNOWN;
    }

    return NABTO_EC_OK;
}

void nm_zephyr_net_deinit(struct nm_zephyr_net* ctx)
{
    nm_zephyr_net_stop(ctx);

    if (ctx->running) {
        k_thread_join(&ctx->threadData, K_FOREVER);
        ctx->running = false;
    }
    zsock_close(ctx->pipefd[0]);
    zsock_close(ctx->pipefd[1]);
}

void nm_zephyr_net_run(struct nm_zephyr_net* ctx)
{
    k_thread_create(&ctx->threadData, ctx->threadStack, K_KERNEL_STACK_SIZEOF(ctx->threadStack), network_thread,
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
    nfds = zsock_select(NP_MAX(ctx->maxReadFd, ctx->maxWriteFd)+1, &ctx->readFds, &ctx->writeFds, NULL, NULL);
    if (nfds < 0) {
        NABTO_LOG_ERROR(LOG, "Error in select: (%i) '%s'", errno, strerror(errno));
    } else {
        NABTO_LOG_TRACE(LOG, "select returned with %i file descriptors", nfds);
    }
    return nfds;
}

int nm_zephyr_net_timed_wait(struct nm_zephyr_net* ctx, uint32_t ms)
{
    NABTO_LOG_TRACE(LOG, "timed wait %d", ms);
    int nfds;
    struct timeval timeout_val;
    timeout_val.tv_sec = (ms/1000);
    timeout_val.tv_usec = ((ms)%1000)*1000;
    build_fd_sets(ctx);
    nfds = zsock_select(NP_MAX(ctx->maxReadFd, ctx->maxWriteFd)+1, &ctx->readFds, &ctx->writeFds, NULL, &timeout_val);
    if (nfds < 0) {
        NABTO_LOG_ERROR(LOG, "Error in select wait: (%i) '%s'", errno, strerror(errno));
    }
    return nfds;
}

void nm_zephyr_net_read(struct nm_zephyr_net* ctx, int nfds)
{
    char one;
    NABTO_LOG_TRACE(LOG, "read: %i", nfds);

    if (ZSOCK_FD_ISSET(ctx->pipefd[0], &ctx->readFds)) {
        (void)zsock_recv(ctx->pipefd[0], &one, 1, 0);
    }
    if (ZSOCK_FD_ISSET(ctx->pipefd[1], &ctx->readFds)) {
        (void)zsock_recv(ctx->pipefd[1], &one, 1, 0);
    }

    nm_zephyr_net_udp_handle_select(ctx, nfds);
    nm_zephyr_net_tcp_handle_select(ctx, nfds);
}

/**
 * Helper functions start
 */

void build_fd_sets(struct nm_zephyr_net* ctx)
{
    ZSOCK_FD_ZERO(&ctx->readFds);
    ZSOCK_FD_ZERO(&ctx->writeFds);
    ctx->maxReadFd = 0;
    ctx->maxWriteFd = 0;
    ZSOCK_FD_SET(ctx->pipefd[0], &ctx->readFds);
    ctx->maxReadFd = NP_MAX(ctx->maxReadFd, ctx->pipefd[0]);
    ZSOCK_FD_SET(ctx->pipefd[1], &ctx->readFds);
    ctx->maxReadFd = NP_MAX(ctx->maxReadFd, ctx->pipefd[1]);

    // ensure there isn't other threads manipulating the list of
    // sockets.
    nm_zephyr_net_udp_build_fd_sets(ctx);

    nm_zephyr_net_tcp_build_fd_sets(ctx);
}

void nm_zephyr_net_notify(struct nm_zephyr_net* ctx)
{
    (void)zsock_send(ctx->pipefd[1], "1", 1, 0);
}

void network_thread(void* data, void* data2, void* data3)
{
    struct nm_zephyr_net* ctx = data;
    while(true) {
        int nfds;
        nm_zephyr_net_lock(ctx);
        bool stopped = ctx->stopped;
        nm_zephyr_net_unlock(ctx);
        if (stopped) {
            return;
        } else {
            // Wait for events.
            nfds = nm_zephyr_net_inf_wait(ctx);
            nm_zephyr_net_read(ctx, nfds);
        }
    }
}



void nm_zephyr_net_lock(struct nm_zephyr_net* ctx)
{
    k_mutex_lock(&ctx->mutex, K_FOREVER);
}

void nm_zephyr_net_unlock(struct nm_zephyr_net* ctx)
{
    k_mutex_unlock(&ctx->mutex);
}
