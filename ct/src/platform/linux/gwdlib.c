/**
 * @brief Dynamic liabrary common functions
 *
 * @file
 * @author lilei (2010-11-02)
 *
 * @addtogroup GW_COMMON_FUNC_LIB Common Function Library
 * @{
 */
#include <dlfcn.h>
#include <gwdlib.h>
#include <log.h>

/**
 * @brief Open a dynamic shared library and return a handle that can be passed to `gw_dlib_sym' to get symbol values from it.
 *
 * @param path path to the dynamic library
 *
 * @return pointer to the memory block on success, NULL on failure
 */
gw_dlib_t gw_dlib_open(const char_t *path)
{
	gw_dlib_t dlhandle = NULL;
	if (path == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_ERROR, "Dynamic library open error, the parameter path is NULL!\n");
	}
	else
	{
		dlhandle = (gw_dlib_t) dlopen(path, RTLD_LAZY);
		if (dlhandle == NULL)
		{
			gw_log_print(GW_LOG_LEVEL_ERROR, "Dynamic library open error!\n");
		}
	}
	return dlhandle;
}

/**
 * @brief  Find the run-time address in the shared object handle refers to of the symbol called sym.
 *
 * @param handle handle to the dl
 * @param sym name of the function in the dl
 *
 * @return pointer to the function on success, NULL on failure
 */
void *gw_dlib_sym(gw_dlib_t handle, const char_t *sym)
{
	void *tmp_pt = NULL;

	if ((handle == NULL) || (sym == NULL))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Dynamic library get function error, the handle is NULL!\n");
	}
	else
	{
		tmp_pt = dlsym(handle, sym);
		if (tmp_pt == NULL)
		{
			gw_log_print(GW_LOG_LEVEL_WARNING, "Dynamic library get function error!\n");
		}
	}
	return tmp_pt;
}

/**
 * @brief Unmap and close a shared object opened by `gw_dlib_open'.
 *
 * @param handle pointer to the dynamic lib
 *
 * @return TRUE on success, FALSE on failure
 */
bool_t gw_dlib_close(gw_dlib_t handle)
{
	bool_t ret = TRUE;
	if (handle == NULL)
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Dynamic library close error, handle is NULL!\n");
		ret = FALSE;
	}
	else if (dlclose(handle))
	{
		gw_log_print(GW_LOG_LEVEL_WARNING, "Dynamic library close error!\n");
	}
	return ret;
}

/**
 * @brief  Output the last dynamic library call error.
 * When any of the dlib functions fails, call this function
 * to return a string describing the error.  Each call resets
 * the error string so that a following call returns null.
 *
 * @param   Non
 *
 * @return  a string describes the error on success, NULL on failure
 */
const char_t * gw_dlib_error(void)
{
	return (dlerror());
}

/**
 * @}
 */
