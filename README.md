# Initialise project using git clone first

```
mkdir zephyrworkspace
cd zephyrworkspace
git clone https://github.com/nabto/edge-zephyr
cd edge-zephyr
west inti -l .
```


# Demo application

Since we need to use MDNS and internet access for a good demo it is easier to
use a development board with Ethernet than using a qemu emulator and have the
hassle with trying to configure it to provide a good user experience.

```
west build -p always -b stm32f746g_disco app
west flash
```

Open a terminal and use the zephyr shell.
