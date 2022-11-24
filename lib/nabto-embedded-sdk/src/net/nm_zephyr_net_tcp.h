#ifndef _nm_zephyr_net_TCP_H_
#define _nm_zephyr_net_TCP_H_

#include "nm_zephyr_net.h"
#include <zephyr/net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

void nm_zephyr_net_tcp_build_fd_sets(struct nm_zephyr_net* ctx);
void nm_zephyr_net_tcp_handle_select(struct nm_zephyr_net* ctx, int nfds);

/**
 * return true iff the socket wants to be informed on events.
 */
bool nm_zephyr_net_tcp_add_to_poll(struct np_tcp_socket* udpSocket, struct zsock_pollfd* pollfd);
void nm_zephyr_net_tcp_handle_poll_events(struct np_tcp_socket* udpSocket, struct zsock_pollfd* pollfd);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
