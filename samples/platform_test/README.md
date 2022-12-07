# Platform test of the nabto integration


## Running on native_posix

west build -b native_posix samples/platform_test

Setup a tap interface called zeth and create some masquerading firewall rules. `./setup-net.sh`.

## Running on stm32f746g_disco

west build -b stm32f746g_disco samples/platform_test

west flash


## Using the shell

```
uart:~$ nabto test
test - Test Nabto integration.
Subcommands:
  udp             :Test platform UDP integration. Args ip, port.
  tcp             :Test platform TCP integration. Args ip, port.
  logging         :Test platform logging integration.
  future_resolve  :Test platform future resolve integration.
  event_queue     :Test platform event queue integration.
  timestamp       :Test platform timestamp integration.
  dns             :Test platform dns integration.
  local_ip        :Test platform local ip integration.
  mdns_publish    :Test platform publish of mdns service.
uart:~$ nabto test udp 192.168.1.163 1235
test udp 192.168.1.163
udp test passed
uart:~$
```
