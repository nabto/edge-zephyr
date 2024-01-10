/* main.c - Copyright Svep Design Center AB */

/**** Includes ****/
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include "mbedtls/entropy.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/sntp.h>
#include <date_time.h>
#include <pthread.h>
#include "modem.h"
#include "cloud.h"
/**** Defines ****/
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);
#define MY_STACK_SIZE 8192
#define MY_PRIORITY 1
K_THREAD_STACK_DEFINE(my_stack_area, MY_STACK_SIZE);


/**** Types ****/


/**** Data ****/
static int64_t m_curr_time_ms = 0;
static int m_lte_connected = 0;
static struct k_thread cloud_thread;


/**** Private functions ****/
static void date_time_evt_handeler(const struct date_time_evt *evt)
{
    if (evt == NULL) {
        printf("Error: Event pointer is NULL\n");
        return;
    }
    int err;
    switch (evt->type) {
        case DATE_TIME_OBTAINED_MODEM:
            LOG_INF("Date and time obtained from modem.\n");
            break;
        case DATE_TIME_OBTAINED_NTP:
            LOG_INF("Date and time obtained from NTP servers.\n");
            err = date_time_now(&m_curr_time_ms);
            if (err != 0 || !date_time_is_valid()) {
                LOG_WRN("Counld not get current tine. Error: %d \n", err);
            } else if (err == 0) {
                LOG_INF("Current time: %lld \n", m_curr_time_ms);
            }
            break;
        case DATE_TIME_OBTAINED_EXT:
            LOG_INF("Date and time obtained from external source.\n");
            break;
        case DATE_TIME_NOT_OBTAINED:
            LOG_INF("Date and time not obtained.\n");
            break;
        default:
            LOG_INF("Unknown date_time event type.\n");
            break;
    }
}

static void time_get()
{
    struct sntp_time current_time;

    /* Server address */
    const char *server_addr = "sth1.ntp.se";

    /* Timeout for SNTP query (in milliseconds) */
    uint32_t timeout = 5000;

    /* Perform SNTP query */
    int ret = sntp_simple(server_addr, timeout, &current_time);

    if (ret < 0) {
        LOG_WRN("SNTP query failed: %d\n", ret);
    } else {
        /* Convert SNTP time to standard UNIX time */
        time_t unix_time = (time_t)(current_time.seconds);

        /* Convert fractional seconds to milliseconds */
        uint32_t milliseconds = (uint32_t)((float)current_time.fraction * 1.0e3 / (float)(1LL << 32));
        LOG_INF("milliseconds %d \n", milliseconds);
        LOG_INF("frac: %d\n", (uint32_t)current_time.fraction);
        LOG_INF("Current time: %s.%03u\n", ctime(&unix_time), milliseconds);
    }

}

static void modem_callback(modem_event_t event, uint32_t payload)
{
    switch (event)
    {
        case MODEM_EVENT_CONNECTING:
            LOG_INF("Connecting! \n");
            break;
        case MODEM_EVENT_CONNECTED:
            //m_app_event = APP_EVENT_CONNECTED;
            if (m_lte_connected)
            {
                break;
            }
            m_lte_connected = 1;
            LOG_INF("Connected!\n");
            //int err = date_time_update_async(date_time_evt_handeler);
            //if (err == 0)
            //{
            //    LOG_INF("Date Time is enabled\n");
            //    date_time_register_handler(date_time_evt_handeler);
            //}
            //else
            //{
            //    LOG_INF("Could not get time, err: %d \n", err);
            //}
            //time_get();
            //cloud_init(); // Initialize the cloud
            break;
        default:
            printk("Unknow event");
            break;
    }
}

void start_cloud_thread(void *arg, void* arg2, void* arg3) {


    cloud_init();

    return;
}

/**** Public functions ****/
int main()
{
    int ret = modem_init(modem_callback);
    if (ret == 0)
    {
        //k_sleep(K_MSEC(10000));
        ret = modem_connect();
    }
    else
    {
        LOG_INF("Modem could not be initialized");
    }

    if (ret == 0)
    {
        LOG_INF("Modem connects \n");
    }
    else
    {
        LOG_INF("Modem could not conenct, err: %d \n", ret);
    }

    k_sleep(K_MSEC(5000));
    LOG_INF("Testing Cloud! \n");

    k_tid_t my_tid = k_thread_create(&cloud_thread, my_stack_area,
                                     K_THREAD_STACK_SIZEOF(my_stack_area),
                                     start_cloud_thread,
                                     NULL, NULL, NULL,
                                     MY_PRIORITY, 0, K_NO_WAIT);

    printk("Thread created\n");

    return 0;
}


int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
    for (size_t i = 0; i < len; i++) {
        output[i] = 0;
    }
    *olen = len;
    return 0;
}
