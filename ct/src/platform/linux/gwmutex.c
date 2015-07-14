/**
 * @brief Mutex control common functions
 *
 * @file
 * @author lilei (2010-11-02)
 *
 * @addtogroup GW_COMMON_FUNC_LIB Common Function Library
 * @{
 */
#include <pthread.h>
#include <gwmutex.h>
#include <log.h>

/**
 * @brief  Create a mutex.
 *
 * @param   NON
 *
 * @return  pointer to the mutex created on success, NULL on failure
 */
gw_mutex_t gw_mutex_create(void)
{
	pthread_mutex_t *mutex;
	mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));

	if (mutex == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_CRITICAL, "Mutex create error, not enough memory!\n");
	}
	else if (pthread_mutex_init(mutex, NULL))
	{
		free(mutex);
		mutex = NULL;
		gw_log_print(GW_LOG_LEVEL_ERROR, "Mutex create error!\n");
	}
	return ((gw_mutex_t) mutex);
}

/**
 * @brief Destroy a mutex.
 *
 * @param mutex pointer to the mutex to be destroyed
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_mutex_destroy(gw_mutex_t mutex)
{
	bool_t ret = TRUE;

	if (mutex == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mutex destroy error, mutex pointer is NULL!\n");
		ret = FALSE;
	}
	else if (pthread_mutex_destroy((pthread_mutex_t *) mutex))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mutex destroy error!\n");
		ret = FALSE;
	}
	free(mutex);
	return ret;
}

/**
 * @brief Lock a mutex.
 *
 * @param mutex pointer to the mutex to be locked
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_mutex_lock(gw_mutex_t mutex)
{
	bool_t ret = TRUE;
	if (mutex == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mutex lock error, mutex pointer is NULL!\n");
		ret = FALSE;
	}
	else if (pthread_mutex_lock((pthread_mutex_t *) mutex))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mutex lock error!\n");
		ret = FALSE;
	}
	return ret;
}

/**
 * @brief Unlock a mutex.
 *
 * @param mutex pointer to the mutex to be unlocked
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_mutex_unlock(gw_mutex_t mutex)
{
	bool_t ret = TRUE;
	if (mutex == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mutex unlock error, mutex pointer is NULL!\n");
		ret = FALSE;
	}
	else if (pthread_mutex_unlock((pthread_mutex_t *) mutex))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Mutex unlock error!\n");
		ret = FALSE;
	}
	return ret;
}

/**
 * @}
 */
