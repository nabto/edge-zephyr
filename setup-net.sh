#!/bin/bash
#
# Copyright (c) 2018 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

usage() {
    cat <<EOF
$0 [--config|-c <file>] [--iface|-i <interface>] [start|up] [stop|down]
               [any optional parameters for ip command]

If no parameters are given, then "zeth" network interface and "zeth.conf"
configuration file are used. The script waits until user presses CTRL-c
and then removes the network interface.

Examples:

$ net-setup.sh
$ net-setup.sh --config zeth-vlan.conf
$ net-setup.sh --config my-own-config.conf --iface foobar

It is also possible to let the script return and then stop the network
interface later. Is can be done by first creating the interface with
"start" or "up" command, and then later remove the interface with
"stop" or "down" command.

$ net-setup.sh start
do your things here
$ net-setup.sh stop

$ net-setup.sh --config my-own-config.conf up
do your things here
$ net-setup.sh --config my-own-config.conf down

Any extra parameters that the script does not know, are passed directly
to "ip" command.

$ net-setup.sh --config my-own-config.conf --iface foo user bar

EOF
    exit
}

if [ "$1" = "-h" -o "$1" = "--help" ]; then
    usage
fi

if [ `id -u` != 0 ]; then
    echo "Run this script as a root user!"
    sudo $0 $@
    exit
fi

IFACE=zeth

INTERFACE=${IFACE}

HWADDR="00:00:5e:00:53:ff"

IPV6_ADDR_1="2001:db8::2"
IPV6_ROUTE_1="2001:db8::/64"

IPV4_ADDR_1="192.0.2.2/24"
IPV4_ROUTE_1="192.0.2.0/24"


STOPPED=0
trap ctrl_c INT TERM

ctrl_c() {
    STOPPED=1
}

if [ "$ACTION" != stop ]; then
    echo "Creating $IFACE"
    ip tuntap add $IFACE mode tap $@

	ip link set dev $INTERFACE up

	ip link set dev $INTERFACE address $HWADDR

	ip -6 address add $IPV6_ADDR_1 dev $INTERFACE nodad
	ip -6 route add $IPV6_ROUTE_1 dev $INTERFACE
	ip address add $IPV4_ADDR_1 dev $INTERFACE
	ip route add $IPV4_ROUTE_1 dev $INTERFACE > /dev/null 2>&1

	sysctl -w net.ipv4.ip_forward=1
    iptables -t nat -A PREROUTING -j ACCEPT
    iptables -A FORWARD -j ACCEPT
    iptables -t nat -A POSTROUTING --out-interface  wlp61s0 -j MASQUERADE
fi

if [ "$ACTION" = "" ]; then
    while [ $STOPPED -eq 0 ]; do
	sleep 1d
    done
fi

if [ "$ACTION" != start ]; then
    iptables -t nat -D POSTROUTING --out-interface  wlp61s0 -j MASQUERADE
    iptables -D FORWARD -j ACCEPT
    iptables -t nat -D PREROUTING -j ACCEPT
    ip link set $IFACE down

    if [ -f "$CONF_FILE".stop ]; then
	. "$CONF_FILE".stop $IFACE
    fi

    echo "Removing $IFACE"
    ip tuntap del $IFACE mode tap
fi
