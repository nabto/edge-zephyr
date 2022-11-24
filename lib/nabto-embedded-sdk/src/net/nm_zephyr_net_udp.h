#ifndef _nm_zephyr_net_UDP_H_
#define _nm_zephyr_net_UDP_H_

#include "nm_zephyr_net.h"

#include <platform/np_platform.h>

#ifdef __cplusplus
extern "C" {
#endif

void nm_zephyr_net_udp_build_fd_sets(struct nm_zephyr_net* ctx);
void nm_zephyr_net_udp_handle_select(struct nm_zephyr_net* ctx, int nfds);


int nm_zephyr_net_udp_nonblocking_socket(int domain, int type);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
