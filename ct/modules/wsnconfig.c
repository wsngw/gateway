#include <stdio.h>
#include <stdlib.h>
#include <dotconf/dotconf.h>
#include <gw.h>

#include <dotconf/libpool.h>
#include "mod_wsnadaptor.h"

/*
 * this is a simple sample structure that we use to keep track
 * of the current context in the configfile when reading it.
 *
 * It just has a field for the currently granted permissions.
 * Later, when the option-table is defined, we'll specify the
 * the needed permissions for each option.
 */
struct mycontext {
	int64_t cur_context;
	int32_t idx;
	char_t current_end_token[256];

	pool_t *pool;
};

struct ptr_list {
	int32_t n_entries;
	void **entries;
};


static DOTCONF_CB(section_open);
static DOTCONF_CB(section_close);
static DOTCONF_CB(common_option);
static DOTCONF_CB(addmodule_option);

extern wsnadpt_conf_t wsnadpt_conf;
struct ptr_list memory_junk; // keep malloc option adress, used to free up them

/*
 * the last field is used to specify the permissions needed for
 * each option. These will be checked at runtime by our contextchecker
 */
static const configoption_t options[] = {
	{"AddModule", ARG_LIST, addmodule_option, NULL, 0},
	{"BaudRate", ARG_INT, common_option, NULL, 0},
	{"DstSrvID", ARG_INT, common_option, NULL, 0},
	{"device", ARG_NAME, common_option, NULL, 0},
	LAST_OPTION
};

const char_t *context_checker_wsnadpt(command_t * cmd, uint64_t mask)
{
	struct mycontext *context = cmd->context;

	return NULL;
}

FUNC_ERRORHANDLER(error_handler_wsnadpt)
{
	fprintf(stderr, "[error] %s\n", msg);

	/* continue reading the configfile ; return 1 stop after first error found */
	return 0;
}


int32_t wsnadpt_config(int8_t *path_filename)
{
	configfile_t *configfile;
	struct mycontext context;
	int32_t i;

	if (path_filename == NULL) {
		printf("error: NULL input\n");
		return 1;
	}

	context.cur_context = 0;
	context.idx = 0;
	context.current_end_token[0] = '\0';
	context.pool = pool_new(NULL);
	wsnadpt_conf.num = 0;
	for(i=0; i< MAX_WSN; i++)
	{
		wsnadpt_conf.config[i].DstSrvID = 0;
		wsnadpt_conf.config[i].wsn_name[0] = '\0';
		wsnadpt_conf.config[i].baudrate = 0;
		wsnadpt_conf.config[i].uartport = 0;
	}

	configfile =
	    dotconf_create(path_filename, options, (void *)&context, CASE_INSENSITIVE);
	if (!configfile) {
		fprintf(stderr, "Error opening configuration file\n");
		return 1;
	}
//	configfile->errorhandler = (dotconf_errorhandler_t) error_handler_wsnadpt;
//	configfile->contextchecker = (dotconf_contextchecker_t) context_checker_wsnadpt;
	if (dotconf_command_loop(configfile) == 0)
		fprintf(stderr, "Error reading configuration file\n");

	dotconf_cleanup(configfile);
	pool_free(context.pool);

	return 0;
}

DOTCONF_CB(section_open)
{
	struct mycontext *context = (struct mycontext *)ctx;
	const char_t *old_end_token = context->current_end_token;
	int old_override = context->cur_context;
	const char_t *err = 0;

	context->cur_context =  cmd->option->context;
	sprintf(context->current_end_token, "</%s", cmd->name + 1);

	while (!cmd->configfile->eof) {
		err = dotconf_command_loop_until_error(cmd->configfile);
		if (!err) {
			err = "</SomeSection> is missing";
			break;
		}

		if (err == context->current_end_token)
			break;

		dotconf_warning(cmd->configfile, DCLOG_ERR, 0, err);
	}

	sprintf(context->current_end_token,"%s",old_end_token);
	context->cur_context = old_override;

	if (err != context->current_end_token)
		return err;

	return NULL;
}


DOTCONF_CB(section_close)
{
	struct mycontext *context = (struct mycontext *)ctx;

	if (!context->current_end_token)
		printf("missing end_token");
	context->idx ++;

	if (strcmp(context->current_end_token, cmd->name)!=0)
		printf("cmd->name and current_end_token don't match\n ");

	return context->current_end_token;
}


/*
 * We expect  option   name
 *     e.g.  AddModule wsnmodule_name
 * So in the list :
 *  data.list[0] -> name
*/
DOTCONF_CB(addmodule_option)
{
	struct config_context *context = (struct config_context *)ctx;
	int32_t idx;

	const char_t *error = 0;

	fprintf(stderr, "addmodule_option : Option %s called\n", cmd->name);

	idx = wsnadpt_conf.num;
	if((idx+1) >= MAX_WSN)
	{
		printf("Too many WSN, quit!\n");
		return NULL;
	}

	wsnadpt_conf.num++;

	/*
	 *   Scope the options of this module to a <NAME></NAME> block where NAME is
	 *   the name that was specified in the configfile.
	 */

	{
		char_t *begin_context_token =
		    (char_t *)malloc(strlen(cmd->data.list[0]) + 2 + 1);  // <NAME>, <> take 2 character, '\0' take one
		char_t *end_context_token =
		    (char_t *)malloc(strlen(cmd->data.list[0]) + 3 + 1); // </NAME>, </> take 3 character,'\0' take one
		configoption_t *scope_options = 0;


		scope_options =
		    (configoption_t *) malloc(3 * sizeof(configoption_t));
		if (!scope_options || !begin_context_token || !end_context_token) {
			return "Error allocating memory";
		}
		sprintf(begin_context_token, "<%s>", cmd->data.list[0]);
		sprintf(end_context_token, "</%s>", cmd->data.list[0]);

		// create our two extra options (scope begin/end) and a NULL option to close the list
		scope_options[0].name = begin_context_token;
		scope_options[0].type = ARG_NONE;
		scope_options[0].callback = section_open;
		scope_options[0].context = (long)idx;

		scope_options[1].name = end_context_token;
		scope_options[1].type = ARG_NONE;
		scope_options[1].callback = section_close;
		scope_options[1].info = NULL;
		scope_options[1].context = (long)idx;

		scope_options[2].name = "";
		scope_options[2].type = 0;
		scope_options[2].callback = NULL;
		scope_options[2].info = NULL;
		scope_options[2].context = 0;

		memory_junk.entries = realloc(memory_junk.entries, (memory_junk.n_entries + 3) * sizeof(void *));
		memory_junk.entries[memory_junk.n_entries++] = begin_context_token;
		memory_junk.entries[memory_junk.n_entries++] = end_context_token;
		memory_junk.entries[memory_junk.n_entries++] = scope_options;

		dotconf_register_options(cmd->configfile, scope_options);
	}

	fprintf(stderr, "Successfully loaded module %s (%s)\n",	cmd->data.list[0], cmd->data.list[0]);
	return NULL;
}

DOTCONF_CB(common_option)
{
	int32_t i, arg_count;
	char_t *command;
	char_t *tmpstr;
	int32_t idx;
	int32_t tmp;
	int8_t *ret = NULL;
	struct mycontext *context = cmd->context;

	printf("config name: \"%s\". Got %d args\n", cmd->name, cmd->arg_count);

	for (i = 0; i < cmd->arg_count; i++)
		printf("\tArg #%d: %s\n", i, cmd->data.list[i]);

	command = cmd->name;
	arg_count = cmd->arg_count;

	if ( strcmp(command, "device") == 0 )
	{
		tmpstr = cmd->data.list[0];
		if ( strcmp(tmpstr, "ttyS0") == 0 )
			tmp = 1;
		else if ( strcmp(tmpstr, "ttyS1") == 0 )
			tmp = 2;
		else if ( strcmp(tmpstr, "ttyS2") == 0 )
			tmp = 3;
		else
			tmp = 1;

		idx = context->idx;
		wsnadpt_conf.config[idx].uartport = tmp;

		printf("the uart is %s\n", tmpstr);
	}

	if ( strcmp(command, "BaudRate") == 0 )
	{
		idx = context->idx;
		wsnadpt_conf.config[idx].baudrate = cmd->data.value;
		printf("the baudrate is set to %d\n", wsnadpt_conf.config[idx].baudrate);
	}

	if ( strcmp(command, "DstSrvID") == 0 )
	{
		idx = context->idx;
		wsnadpt_conf.config[idx].DstSrvID = cmd->data.value;
		printf("the DstSrvID is set to %d\n", wsnadpt_conf.config[idx].DstSrvID);
	}

	return NULL;
}
