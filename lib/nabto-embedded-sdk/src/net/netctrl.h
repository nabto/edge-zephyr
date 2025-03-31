#ifndef NETCTRL_H__
#define NETCTRL_H__

/**
*  @file netctrl.h
*  @brief 
* 
*  @copyright Copyright Svep Design Center AB 2025
*/

/**** Includes ****/

/**** Defines ****/

/**** Types ****/

/**** Data ****/

/**** Public functions ****/
#include <stdbool.h>
void nm_zephyr_set_conn(bool is_conn);
bool nm_zephyr_get_conn(void);

#endif

