#include <zephyr/net/net_if.h>

#include <stdio.h>

void main(void) {
    struct net_if *iface;
    iface = net_if_get_default();
    net_dhcpv4_start(iface);
}
