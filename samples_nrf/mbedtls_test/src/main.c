#include <zephyr/logging/log.h>

#include <mbedtls/ssl.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main()
{
    const char *alpnList[2] = {"foo", NULL };
    mbedtls_ssl_conf_alpn_protocols(NULL, alpnList);
    LOG_INF("int main().\n");
    return 0;
}
