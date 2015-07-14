/**
 * @brief Memory pool process functions
 *
 * @file
 * @author lilei (2010-11-15)
 *
 * @addtogroup GW_MEM_POOL Mempool Control Function
 * @{
 */

#include <mempool.h>
#include <log.h>

/**
 * @brief Create a memory pool with 'num' blocks, each block has a 'size' size.
 *
 * @param num the number of blocks in the memory pool
 * @param size the size of each memory block
 *
 * @return a pointer to the head of the memory pool on success, NULL on failure
 */
mp_head_pt gw_mem_pool_create(uint32_t num, uint32_t size)
{
	mp_head_pt head;
	int32_t i, j;
	void *tmp_pt;

	if ((num == 0) || (size == 0))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mem pool create failed, passed num is 0!\n");
	}
	else
	{
		head = (mp_head_pt) malloc(sizeof(mp_head_t));
		if (head == NULL)
		{
			gw_log_print(GW_LOG_LEVEL_CRITICAL, "Mem pool create failed, not enouth memory!");
		}
		else
		{
			head->next = NULL;
			head->size = size;
			head->num = num;
			head->mutex = gw_mutex_create();
			head->avil = 0;

			for (i = 0; i < head->num; i++)
			{
				tmp_pt = malloc(head->size);
				if (tmp_pt == NULL)
				{
					//not enough memory!
					gw_log_print(GW_LOG_LEVEL_CRITICAL, "Mem pool create failed, not enouth memory!\n");
					for (j = 0; j < head->avil; j++)
					{
						tmp_pt = *((void **) head->next);
						free(head->next);
						head->next = tmp_pt;
					}
					gw_mutex_destroy(head->mutex);
					free(head);
					head = NULL;
					break;
				}
				*((void **) tmp_pt) = head->next;
				head->next = tmp_pt;
				head->avil++;
			}
		}
	}
	return head;
}

/**
 * @brief Destroy a memory pool.
 *
 * @param head pointer to the head of the memory pool
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_mem_pool_destroy(mp_head_pt head)
{
	int32_t j;
	void *tmp_pt;
	bool_t ret = TRUE;
	printf("mem pool start to destroy! total %d, avil %d\n", head->num, head->avil);
	if (head == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mem pool destroy failed, try to destroy a memory pool does not exist!\n");
		return FALSE;
	}
	if (head->num != head->avil)
	{
		//not all the memory block are freed
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mem pool destroy warning, %d blocks are not returned to the memory pool!\n", head->num - head->avil);
	}
	gw_mutex_lock(head->mutex);
	for (j = 0; j < head->avil; j++)
	{
		tmp_pt = *((void **) head->next);
		free(head->next);
		head->next = tmp_pt;
	}
	gw_mutex_unlock(head->mutex);
	gw_mutex_destroy(head->mutex);
	free(head);
	return ret;
}

/**
 * @brief Allocate a memory block to the caller.
 *
 * @param head pointer to the head of the memory pool
 *
 * @return a pointer to the allocated memory block on success, NULL on failure
 */
void *gw_mem_pool_alloc(mp_head_pt head)
{
	void *tmp_pt;
	if (head == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mem pool allocte error, try to alloc a block from a memory pool does not exist!\n");
		return NULL;
	}
	printf("mem pool is taken by 1, total %d, avil %d\n",head->num, head->avil);
	if (head->avil > 0)
	{
		gw_mutex_lock(head->mutex);
		tmp_pt = head->next;
		head->next = *((void **) tmp_pt);
		head->avil--;
		gw_mutex_unlock(head->mutex);
		memset(tmp_pt, 0, head->size);
		return tmp_pt;
	}
	else
	{
		tmp_pt = malloc(head->size);
		if (tmp_pt == NULL)
		{
			//not enough memory!
			gw_log_print(GW_LOG_LEVEL_CRITICAL, "Mem pool allocte failed, not enouth memory!");
			return NULL;
		}
		gw_mutex_lock(head->mutex);
		head->num++;
		gw_mutex_unlock(head->mutex);
		gw_log_print(GW_LOG_LEVEL_INFO, "Mem pool enlarges, now size is %d!\n", head->num);
		return tmp_pt;
	}
	printf("mem pool is taken by 1, total %d, avil %d\n", head->num, head->avil);
}

/**
 * @brief Return a block to the memory pool.
 *
 * @param head the pointer to the head of the memory pool
 * @param block pointer to the returned block
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_mem_pool_free(mp_head_pt head, void *block)
{
	//	void *tmp_pt;
	if (head == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mem pool free error, try to release a block to a memory pool does not exist!\n");
		return FALSE;
	}
	if (block == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mem pool free error, try to release a NULL pointer!\n");
		return FALSE;
	}
	printf("gw mem pool free is called once, avil is %d!\n",head->avil);
	gw_mutex_lock(head->mutex);
	*((void **) block) = head->next;
	head->next = block;
	head->avil++;
	gw_mutex_unlock(head->mutex);
	block = NULL;
	printf("gw mem pool free is called second, avil is %d!\n",head->avil);
	return TRUE;
}

/**
 * @}
 */
