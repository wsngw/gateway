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
	int32_t uartport;
	int32_t baudrate;
	char * wsn_name[32];
} ;

typedef struct __tag_wsnadpt_config
{
	int num;
	struct uartconf config[MAX_WSN];

}wsnadpt_conf_t;


#endif /* MOD_WSNADAPTOR_H_ */
