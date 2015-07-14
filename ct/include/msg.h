/*
 * msg.h
 *
 *  Created on: 2010-11-1
 *      Author: makui
 *   Copyright: 2010, WSN
 */

#ifndef __MSG_H
#define __MSG_H

#include <gw.h>

/* error code */
#define	EC_SUCCESS			0x0000
#define EC_NORMAL_ERROR		0x0001
#define EC_CRC_ERROR		0x0002
#define EC_PARMA_ERROR		0x0003
#define EC_NETWOEK_ERROR	0x0004

#define EC_DEV_UNJOIN		0x0010
#define EC_DEV_UNREG		0x0011

/* message structure */
typedef struct __tag_gw_msg
{
	struct list_head	header;	//TODO: list head

	/* basic info */
	struct
	{
		uint16_t		major;
		uint16_t		minor;
	} 					version;
	uint32_t			serial;
	uint8_t				devid[8];
	uint16_t			srcsvc;
	uint16_t			dstsvc;
	uint32_t			type;
	uint16_t			retcode;

	/* payload related */
	void				*data;
	size_t				datasize;
} gw_msg_t;

typedef struct __tag_gw_msgq
{
	struct list_head	header;	//TODO: list head
	uint32_t			num;
	gw_sem_t			sem;	//TODO: msgq semaphore
	gw_mutex_t		    lock;	//TODO: msgq mutex lock
} gw_msgq_t;

/* message system init/destroy */
bool_t gw_msg_init(void);
bool_t gw_msg_destroy(void);

/* message alloc/free */
gw_msg_t *gw_msg_alloc(void);
bool_t gw_msg_free(gw_msg_t *msg);

/* message data manipulation */
void *gw_msg_data_alloc(gw_msg_t *msg, size_t len);
bool_t gw_msg_data_free(gw_msg_t *msg);
void *gw_msg_data_get(gw_msg_t *msg, size_t *len);

/* message communication */
bool_t gw_msg_send(gw_t *gw, gw_msg_t *msg);
gw_msg_t *gw_msg_recv(gw_msgq_t *mq);

/* message queue */
gw_msgq_t *gw_msgq_create(void);
bool_t gw_msgq_destroy(gw_msgq_t *mq);

#endif /* __MSG_H */
