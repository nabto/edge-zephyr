#ifndef CLOUD_H__
#define CLOUD_H__

/* modem.h - Copyright Svep Design Center AB */


/**** Includes ****/
#include <stdint.h>

/**** Defines ****/
typedef enum {
    CLOUD_EVENT_GOT_MESSAGE,
    CLOUD_EVENT_CONNECTING,
    CLOUD_EVENT_CONNECTED,
    CLOUD_EVENT_PUBLISHED,
} cloud_event_t;

/**** Types ****/
typedef void (*cloud_event_cb_t)(cloud_event_t event);


/**** Data ****/


/**** Public functions ****/
int cloud_init();

#endif