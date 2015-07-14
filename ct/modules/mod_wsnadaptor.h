/*
 * mod_wsnadaptor.h
 *
 *  Created on: 2010-12-30
 *      Author: makui
 */

#ifndef MOD_WSNADAPTOR_H_
#define MOD_WSNADAPTOR_H_

#include <gwtypes.h>

/* module wsnadaptor config information */
#define MAX_WSN 10

struct uartconf{
	uint32_t uartport;
	uint32_t baudrate;
	uint16_t DstSrvID;

	char_t * wsn_name[32];
} ;

typedef struct __tag_wsnadpt_config
{
	int32_t num;
	struct uartconf config[MAX_WSN];

}wsnadpt_conf_t;


#endif /* MOD_WSNADAPTOR_H_ */
