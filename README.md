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
