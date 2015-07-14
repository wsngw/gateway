/**
 * @brief Semaphore control common functions
 *
 * @file
 * @author lilei (2010-11-02)
 *
 * @addtogroup GW_COMMON_FUNC_LIB Common Function Library
 * @{
 */

#include <semaphore.h>
#include <gwsem.h>
#include <log.h>

/**
 * @brief Create a semaphore and initialize with 'value'.
 *
 * @param value initial value of the semaphore created
 *
 * @return pointer to the semaphore created on success, NULL on failure
 */
gw_sem_t gw_sem_create(uint32_t value)
{
	sem_t *sem = NULL;
	if (value < 0)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Sem create error, invalid value!\n");
	}
	else
	{
		sem = (sem_t *) malloc(sizeof(sem_t));
		if (sem == NULL)
		{
			gw_log_print(GW_LOG_LEVEL_CRITICAL, "Sem create error, not enough memory!\n");
		}
		else if (sem_init(sem, 0, value) != 0)
		{
			free(sem);
			sem = NULL;
			gw_log_print(GW_LOG_LEVEL_ERROR, "Sem create error!\n");
		}
	}
	return sem;
}

/**
 * @brief Destroy a semaphore.
 *
 * @param sem pointer to the semaphore to be destroyed
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_sem_destroy(gw_sem_t sem)
{
	bool_t ret = TRUE;
	if (sem == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Sem destroy error, sem pointer is NULL!\n");
		ret = FALSE;
	}
	else if (sem_destroy((sem_t *) sem))
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "Sem destroy error!\n");
		ret = FALSE;
	}
	free(sem);
	return ret;
}

/**
 * @brief Wait for a semaphore to be posted.
 *
 * @param sem pointer to the semaphore to be waited
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_sem_wait(gw_sem_t sem)
{
	bool_t ret = TRUE;
	if (sem == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Sem wait error, sem pointer is NULL!\n");
		ret = FALSE;
	}
	else if (sem_wait((sem_t *) sem))
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "Sem wait error!\n");
		ret = FALSE;
	}
	return ret;
}

/**
 * @brief Post a semaphore to be available.
 *
 * @param sem pointer to the semaphore to be posted
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_sem_post(gw_sem_t sem)
{
	bool_t ret = TRUE;
	if (sem == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Sem post error, sem pointer is NULL!\n");
		ret = FALSE;
	}
	else if (sem_post((sem_t *) sem))
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "Sem post error!\n");
		ret = FALSE;
	}
	return ret;
}

/**
 * @}
 */
