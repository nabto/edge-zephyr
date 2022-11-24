#ifndef nm_zephyr_net_H
#define nm_zephyr_net_H

#include <platform/np_types.h>
#include <platform/np_platform.h>
#include <api/nabto_device_threads.h>

#include <sys/select.h>

#include <nn/llist.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket_select.h>

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

#define NM_ZEPHYR_NET_STACK_SIZE 512



struct nm_zephyr_net {
    zsock_fd_set readFds;
    zsock_fd_set writeFds;
    int maxReadFd;
    int maxWriteFd;
    int pipefd[2];
    struct nn_llist udpSockets;
    struct nn_llist tcpSockets;

    struct k_thread threadData;
    K_KERNEL_STACK_MEMBER(threadStack, NM_ZEPHYR_NET_STACK_SIZE);
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


#ifdef __cplusplus
} //extern "C"
#endif

#endif // nm_zephyr_net_H
