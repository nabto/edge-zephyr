#ifndef nm_zephyr_net_H
#define nm_zephyr_net_H

#include <platform/np_types.h>
#include <platform/np_platform.h>
#include <api/nabto_device_threads.h>

#include <nn/llist.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket_select.h>
#include <zephyr/net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif


struct nm_zephyr_net_udp_recv_wait_context {
    struct np_completion_event* completionEvent;
};

struct np_udp_socket {
    struct nm_zephyr_net* selectCtx;
    int sock;
    enum np_ip_address_type type;

    struct nn_llist_node udpSocketsNode;
    bool aborted;
    struct nm_zephyr_net_udp_recv_wait_context recv;
};

struct nm_zephyr_net_tcp_connect_context {
    struct np_completion_event* completionEvent;
};

struct nm_zephyr_net_tcp_write_context {
    struct np_completion_event* completionEvent;
    const void* data;
    size_t dataLength;
};

struct nm_zephyr_net_tcp_read_context {
    struct np_completion_event* completionEvent;
    void* buffer;
    size_t bufferSize;
    size_t* readLength;
};

struct np_tcp_socket {
    struct nn_llist_node tcpSocketsNode;
    struct nm_zephyr_net* selectCtx;
    int fd;

    struct nm_zephyr_net_tcp_connect_context connect;
    struct nm_zephyr_net_tcp_write_context write;
    struct nm_zephyr_net_tcp_read_context read;

    bool destroyed;
    bool aborted;
};



enum pollfd_type {
    NM_ZEPHYR_NET_TYPE_UDP,
    NM_ZEPHYR_NET_TYPE_TCP,
    NM_ZEPHYR_NET_TYPE_PIPE
};

struct pollfd_info
{
    enum pollfd_type type;
    void* ptr;
};

struct nm_zephyr_net {
    struct zsock_pollfd pollfds[CONFIG_NET_SOCKETS_POLL_MAX];
    struct pollfd_info pollfdInfo[CONFIG_NET_SOCKETS_POLL_MAX];
    size_t usedPollfds;

    int pipefd[2];
    struct nn_llist udpSockets;
    struct nn_llist tcpSockets;

    //pthread_t thread;
    // synchronize core thread and select thread access to the list of sockets.
    struct k_mutex mutex;
    //pthread_mutex_t mutex;
    bool stopped;
    bool running;
};

/**
 * Functions used from the API
 */
np_error_code nm_zephyr_net_init(struct nm_zephyr_net* ctx);
void nm_zephyr_net_deinit(struct nm_zephyr_net* ctx);

void nm_zephyr_net_run(struct nm_zephyr_net* ctx);
void nm_zephyr_net_stop(struct nm_zephyr_net* ctx);

void nm_zephyr_net_lock(struct nm_zephyr_net* ctx);
void nm_zephyr_net_unlock(struct nm_zephyr_net* ctx);

/**
 * Functions only used internally in the module
 */

// notify select that something has changed in the filedescriptor sets
void nm_zephyr_net_notify(struct nm_zephyr_net* ctx);

/**
 * Get implementations for the implemented modules.
 */

/**
 * Get an object implementing the udp interface.
 */
struct np_udp nm_zephyr_net_udp_get_impl(struct nm_zephyr_net* ctx);

/**
 * Get an object implementing the tcp interface.
 */
struct np_tcp nm_zephyr_net_tcp_get_impl(struct nm_zephyr_net* ctx);

/**
 * Get an object implementing the local ip interface
 */
struct np_local_ip nm_zephyr_net_local_ip_get_impl(struct nm_zephyr_net* ctx);


#ifdef __cplusplus
} //extern "C"
#endif

#endif // nm_zephyr_net_H
