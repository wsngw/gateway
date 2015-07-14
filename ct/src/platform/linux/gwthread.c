/**
 * @brief Thread control common functions
 *
 * @file
 * @author lilei (2010-11-02)
 *
 * @addtogroup GW_COMMON_FUNC_LIB Common Function Library
 * @{
 */
#include <pthread.h>
#include <gwthread.h>
#include <log.h>

/**
 * @brief  Create a thread starting with execution of 'routine' getting passed.
 *  Creation attributed come from ATTR.
 *
 * @param routine pointer to the function to be called by this thread
 * @param arg parameters passed to routine
 *
 * @return  pointer to the thread created on success, NULL on failure
 */
gw_thread_t gw_thread_create(void *(* routine)(void *), void *arg)
{
	pthread_t *pid;

	pid = (pthread_t *) malloc(sizeof(pthread_t));
	if (pid == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_CRITICAL, "Thread create error, not enough memory!\n");
	}
	else if (pthread_create(pid, NULL, routine, arg))
	{
		free(pid);
		pid = NULL;
		gw_log_print(GW_LOG_LEVEL_ERROR, "Thread create error!\n");
	}

	return ((gw_thread_t) pid);
}

/**
 * @brief Exit from a thread.
 *
 * @param rtval exit code passed to any thread successful terminating this thread
 *
 * @return NON
 */
void gw_thread_exit(uint32_t rtval)
{
	pthread_exit((void *) rtval);
}

/**
 * @brief Wait for a thread to terminate.
 *
 * @param handle pointer to the thread waited
 *
 * @return TRUE on success, FALSE on failure
 */
int32_t gw_thread_wait(gw_thread_t handle)
{
	int32_t err;
	pthread_t pid;
	int32_t ret;
	void *tret;

	if (handle == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Thread wait error, invalid handle!\n");
		ret = -1;
	}
	else
	{
		pid = *((pthread_t *) handle);
		err = pthread_join(pid, &tret);
		if (err != 0)
		{
			gw_log_print(GW_LOG_LEVEL_WARNING, "Thread wait error!\n");
			ret = -1;
		}
		else
		{
			free(handle);
			ret = (int32_t) tret;
		}
	}
	return ret;
}
/**
 * @brief Get the handle of the current thread.
 *
 * A malloc fucntion is called in gw_thread_self, need to be released manully by called gw_thread_close()
 *
 * @param NON
 *
 * @return pointer to the thread, NULL on failure
 */
gw_thread_t gw_thread_self(void)
{
	pthread_t pid, *pidt;
	pidt = (pthread_t *) malloc(sizeof(pthread_t));
	if (pidt == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_CRITICAL, "Thread self error, not enough memory!\n");
	}
	else
	{
		pid = pthread_self();
		memcpy(pidt, &pid, sizeof(pthread_t));
	}
	return ((gw_thread_t) pidt);
}

/**
 * @brief Set a thread to detached status.
 *
 * @param handle pointer to the thread
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_thread_detach(gw_thread_t handle)
{
	pthread_t pid;
	bool_t ret = TRUE;

	if (handle == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Thread detach error, invalid handle!\n");
		ret = FALSE;
	}
	else
	{
		pid = *((pthread_t *) handle);
		if (pthread_detach(pid))
		{
			gw_log_print(GW_LOG_LEVEL_ERROR, "Thread detach error!\n");
			ret = FALSE;
		}
		else
		{
			gw_thread_close(handle);
		}
	}
	return ret;
}

/**
 * @brief Release the pointer to a thread.
 *
 * @param handle pointer to a thread
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_thread_close(gw_thread_t handle)
{
	bool_t ret = TRUE;
	if (handle == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Thread close error, invalid handle!\n");
		ret = FALSE;
	}
	else
	{
		free(handle);
	}
	return ret;
}

/**
 * @}
 */
