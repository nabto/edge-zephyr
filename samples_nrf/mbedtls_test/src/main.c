#include <zephyr/logging/log.h>

#include <mbedtls/ssl.h>
#include <mbedtls/check_config.h>
#include <mbedtls/ctr_drbg.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main()
{
    // see that some apis work
    mbedtls_ssl_config config;
    mbedtls_ssl_config_init(&config);
    const char *alpnList[2] = {"foo", NULL};
    // se that alpn does not work
    mbedtls_ssl_conf_alpn_protocols(NULL, alpnList);

    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init( &ctr_drbg );

    LOG_INF("int main().\n");
    return 0;
}
