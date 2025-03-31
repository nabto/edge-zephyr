#ifndef _nm_zephyr_net_UDP_H_
#define _nm_zephyr_net_UDP_H_

#include "nm_zephyr_net.h"

#include <platform/np_platform.h>

#include <zephyr/net/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

void nm_zephyr_net_udp_build_fd_sets(struct nm_zephyr_net* ctx);
void nm_zephyr_net_udp_handle_select(struct nm_zephyr_net* ctx, int nfds);

/**
 * return true iff the socket wants to be informed on events.
 */
bool nm_zephyr_net_udp_add_to_poll(struct np_udp_socket* udpSocket, struct zsock_pollfd* pollfd);
void nm_zephyr_net_udp_handle_poll_events(struct np_udp_socket* udpSocket, struct zsock_pollfd* pollfd);


int nm_zephyr_net_udp_nonblocking_socket(int domain, int type);

np_error_code create_socket_any(struct np_udp_socket* s);
np_error_code nm_zephyr_net_recreate_udp(void);
np_error_code nm_zephyr_net_down(void);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
