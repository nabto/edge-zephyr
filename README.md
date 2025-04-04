# install tools:

Follow the guide at https://docs.zephyrproject.org/latest/develop/getting_started/index.html

# Initialise project using git clone first

```
source ~/zephyrproject/.venv/bin/activate

mkdir zephyrworkspace
cd zephyrworkspace
git clone git@github.com:nabto/edge-zephyr
cd edge-zephyr
west init -l .
```


# Demo application

Since we need to use MDNS and internet access for a good demo it is easier to
use a development board with Ethernet than using a qemu emulator and have the
hassle with trying to configure it to provide a good user experience.

```
west build -p always -b stm32f746g_disco samples/simple_coap
west flash
```

Open a terminal and use the zephyr shell.


# Running on posix_sim

Posix sim a posix implementation of zephyr which can run on. The implementation
uses networking through a layer 3 tap interface and a psuedotty for stdin and
stdout.

Building:

```
west build -p always -b native_posix_64 samples/simple_coap
```

Running:

The demo uses a tap interface the tap interface can be setup with
`./setup-net.sh` the outgoing interface needs to be set to the desired outgoing
interface on the system.

```
west build -t run
```

Screen can be used to connect to the pseudotty such that it is possible to use
the shell. `screen <pseudotty>` e.g. `screen /dev/pts/17`


# Building for Nordic Semiconductor Boards using nRF Connect SDK

The repository https://github.com/nabto/edge-zephyr-nrf contains samples for
Nordic Semiconductor based boards using the nRF Connect SDK.
