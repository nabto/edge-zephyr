#ifndef MODEM_H__
#define MODEM_H__

/* modem.h - Copyright Svep Design Center AB */


/**** Includes ****/
#include <stdint.h>

/**** Defines ****/


/**** Types ****/
typedef enum {
    MODEM_EVENT_DISCONNECTED,
    MODEM_EVENT_CONNECTING,
    MODEM_EVENT_CONNECTED,
    MODEM_EVENT_PUBLISHED,
} modem_event_t;

typedef void (*modem_event_cb_t)(modem_event_t event, uint32_t payload);


/**** Data ****/


/**** Public functions ****/
int modem_init(modem_event_cb_t cb);
int modem_connect(void);

#endif