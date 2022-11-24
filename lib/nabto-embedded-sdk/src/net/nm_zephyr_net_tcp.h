#ifndef _nm_zephyr_net_TCP_H_
#define _nm_zephyr_net_TCP_H_

#include "nm_zephyr_net.h"

#ifdef __cplusplus
extern "C" {
#endif

void nm_zephyr_net_tcp_build_fd_sets(struct nm_zephyr_net* ctx);
void nm_zephyr_net_tcp_handle_select(struct nm_zephyr_net* ctx, int nfds);

#ifdef __cplusplus
} //extern "C"
#endif

#endif
