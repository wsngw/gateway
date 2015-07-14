/**
 * @brief Msg process common functions
 *
 * @file
 * @author lilei (2010-11-11)
 *
 * @addtogroup GW_MSG Message Control Function
 * @{
 */
#include <msg.h>
#include <mempool.h>
#include <module.h>
#include <stdio.h>

static mp_head_pt mphead;
#define MQ_DEFAULT_NUM 50

/**
 * @brief Initiate the msg queue system, primarily create a mem pool for the message queue.
 *
 * @param NON
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_msg_init(void)
{
	bool_t ret = TRUE;
	mphead = gw_mem_pool_create(MQ_DEFAULT_NUM, sizeof(gw_msg_t));
	if (mphead == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "Message queue system initiate error!\n");
		ret = FALSE;
	}
	return ret;
}

/**
 * @brief Destroy the msg queue system.
 *
 * @param NON
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_msg_destroy(void)
{
	bool_t err;
	err = gw_mem_pool_destroy(mphead);
	if (err == FALSE)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Message queue system destroy error!\n");
	}
	return err;
}

/**
 * @brief Allocte a message from the msg queue system.
 *
 * @param NON
 *
 * @return pointer to the msg allocated on success, NULL on failure
 */
gw_msg_t *gw_msg_alloc(void)
{
	gw_msg_t *msg = NULL;
	printf("gw msg alloc is called once!\n");
	msg = gw_mem_pool_alloc(mphead);
	if (msg == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "Message queue system allocate message error!\n");
	}
	else
	{
		msg->data = NULL;
	}

	return msg;
}

/**
 * @brief Return a message to the msg queue system.
 *
 * @param msg pointer to the message
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_msg_free(gw_msg_t *msg)
{
	bool_t err;
	printf("gw msg free is called once!\n");
	if(msg == NULL)
	{
		gw_debug_print("NUll pointer to be freed!\n");
		return FALSE;
	}
	if ((msg->data) != NULL)
	{
		err = gw_msg_data_free(msg);
	}
	err = gw_mem_pool_free(mphead, (void *) msg);
	msg = NULL;
	if (err == FALSE)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Message queue system msg free error!\n");
	}
	return err;
}

/**
 * @brief Allocte data for a msg.
 *
 * @param msg pointer to the message
 * @param len the length of the data to be allocated
 *
 * @return pointer to the data allocated on success, NULL on failure
 */
void *gw_msg_data_alloc(gw_msg_t *msg, size_t len)
{
	void *pt;
	if ((msg == NULL) || (len == 0))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Msg data allocate error, the msg pointer is NULL!\n");
		pt = NULL;
	}
	else
	{
		pt = malloc(len);
		if (pt == NULL)
		{
			gw_log_print(GW_LOG_LEVEL_CRITICAL, "Msg data allocate error, not enough memory!\n");
		}
	}
	msg->data = pt;
	return (msg->data);
}

/**
 * @brief Try to free the data of a message.
 *
 * @param msg pointer to the message
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_msg_data_free(gw_msg_t *msg)
{
	bool_t ret = TRUE;
	if ((msg == NULL) || (msg->data == NULL))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Msg data free error, the msg pointer is NULL!\n");
		ret = FALSE;
	}
	else
	{
		free(msg->data);
		msg->data = NULL;
	}
	return ret;
}

/**
 * @brief Return the data of a msg.
 *
 * @param msg pointer to the message
 * @param len the length of the data to be returned
 *
 * @return pointer to the data of a message on success, NULL on failure
 */
void *gw_msg_data_get(gw_msg_t *msg, size_t *len)
{
	void *pt = NULL;
	if ((msg == NULL) || (len == 0))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Msg data get error, the msg pointer is NULL!\n");
		pt = NULL;
	}
	else
	{
		pt = msg->data;
	}
	return pt;
}

/**
 * @brief Send a message to the msg queue of dst service.
 *
 * @param gw pointer to gateway context
 * @param msg pointer to the msg to be sent
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_msg_send(gw_t *gw, gw_msg_t *msg)
{
	gw_svc_t *svc;
	bool_t ret = TRUE;
	int32_t i;
//	gw_debug_print("send a message!, msg type is %d\n", msg->type);
	if ((gw == NULL) || (msg == NULL))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Msg send error, try to send a NULL message\n");
		ret = FALSE;
	}
	else
	{
		if((msg->dstsvc == SN_DM_SVC) || (msg->dstsvc == SN_DATA_SVC))
		{
			node_data_t *tmp_node_data = node_data_get(msg->devid, NUI_LEN);
			if(tmp_node_data == NULL)
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "msg send error, msg nui is:\n");
//				for(i=0; i<NUI_LEN; i++)
//				{
//					printf("%02x ", (uint8_t)msg->devid[i]);
//				}
//				printf("\n");
				ret = FALSE;
			}
			else
			{
				gw_mod_t *module = (gw_mod_t *)tmp_node_data->pdata->module;
				svc = mod_get_mod_service_first(gw, module);
				while(svc->id != msg->dstsvc)
				{
					svc = mod_get_mod_service_next(gw, module, svc);
				}
			}
		}
		else
		{
			svc = mod_get_service_by_id(gw, msg->dstsvc);
		}

		if(ret)
		{
			if (svc == NULL || svc->mq == NULL)
			{
				gw_log_print(GW_LOG_LEVEL_WARNING, "Msg send error, dst msg queue does not exist!\n");
				ret = FALSE;
			}
			else
			{
				gw_mutex_lock(svc->mq->lock);
				list_add(&(msg->header), &(svc->mq->header));
				svc->mq->num++;
				gw_mutex_unlock(svc->mq->lock);
				gw_sem_post(svc->mq->sem);
			}
		}
	}
	return ret;
}

/**
 * @brief Receive a message from a msg queue.
 *
 * @param mq pointer to message queue
 *
 * @return pointer to a message on success, NULL on failure
 */
gw_msg_t *gw_msg_recv(gw_msgq_t *mq)
{
	gw_msg_t *msg = NULL;

	if (mq == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Msg recv error, src msg queue does not exist!\n");
	}
	else
	{
		gw_sem_wait(mq->sem);
		gw_mutex_lock(mq->lock);
		msg = list_entry(mq->header.prev, gw_msg_t, header);
		list_del(mq->header.prev);
		mq->num--;
		gw_mutex_unlock(mq->lock);
	}
	return msg;
}

/**
 * @brief Create a msg queue.
 *
 * @param NON
 *
 * @return pointer to a message queue on success, NULL on failure
 */
gw_msgq_t *gw_msgq_create(void)
{
	gw_msgq_t *mq = NULL;

	mq = (gw_msgq_t *) malloc(sizeof(gw_msgq_t));
	if (mq == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_CRITICAL, "Msg queue create error, not enough memory!\n");
	}
	else
	{
		INIT_LIST_HEAD(&(mq->header));
		mq->num = 0;
		mq->sem = gw_sem_create(0);
		mq->lock = gw_mutex_create();
	}
	return mq;
}

/**
 * @brief Destroy a msg queue.
 *
 * @param mq pointer to a message queue
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_msgq_destroy(gw_msgq_t *mq)
{
	struct list_head *pos, *tmp_pos;
	gw_msg_t *tmp_msg;
	bool_t ret = TRUE;
	if (mq == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Msg queue destroy error, try to destroy a message queue does not exist!\n");
		ret = FALSE;
	}
	else
	{
		if (mq->num != 0)
		{
			gw_log_print(GW_LOG_LEVEL_WARNING, "Try to destroy a message queue not empty!\n");
			gw_mutex_lock(mq->lock);
			list_for_each_safe(pos, tmp_pos, &(mq->header))
			{
				tmp_msg = list_entry(pos, gw_msg_t, header);
				list_del(pos);
				gw_msg_free(tmp_msg);
			}
			gw_mutex_unlock(mq->lock);
		}
		gw_mutex_destroy(mq->lock);
		gw_sem_destroy(mq->sem);
		free(mq);
		mq = NULL;
	}
	return ret;
}

/**
 * @}
 */
