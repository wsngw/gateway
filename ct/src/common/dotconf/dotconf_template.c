#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dotconf/dotconf.h>

/* declare a callback funtion gwconf_callback() */
static DOTCONF_CB(gwconf_callback);
DOTCONF_CB(default_callback);

// replace gwconf and gwconf_callback with yours
static const configoption_t options[] = {
	{"gwconf", ARG_LIST, gwconf_callback, NULL, CTX_ALL},
	{"", ARG_NAME, default_callback, NULL, 0},    //default callback
	LAST_OPTION
};

// this funtion read all the conf file, parse and process all commands
// you don't need to modify this file
int gwconfig(char *path_filename)
{
	configfile_t *configfile;

	if (path_filename == NULL) {
		printf("error: NULL input\n");
		return 1;
	}
	configfile = dotconf_create(path_filename,
				    options, NULL, CASE_INSENSITIVE);
	if (!configfile) {
		fprintf(stderr, "Error creating configfile\n");
		return 1;
	}

	if (dotconf_command_loop(configfile) == 0) {
		fprintf(stderr, "Error parsing config file %s\n",configfile->filename);
		return 1;
	}

	dotconf_cleanup(configfile);

	return 0;
}

// modify this funtion for your needs
DOTCONF_CB(gwconf_callback)
{
	int i, arg_count;
	char *command, *arg ;

	/*
	printf("%s:%ld: %s: [  ",
	       cmd->configfile->filename, cmd->configfile->line, cmd->name);
	for (i = 0; i < cmd->arg_count; i++)
		printf("(%d) %s  ", i, cmd->data.list[i]);
	printf("]\n");
	*/
	printf("gwconf_callback handler called for \"%s\". Got %d args\n", cmd->name,
	       cmd->arg_count);
	for (i = 0; i < cmd->arg_count; i++)
		printf("\tArg #%d: %s\n", i, cmd->data.list[i]);

	command = cmd->name;
	arg_count = cmd->arg_count;
	if(arg_count <= 1 || arg_count > CFG_VALUES) {
		printf("Incorrect num of input argments!\n");
		return NULL;
	}
	// read all string as input argments
	//		for (i = 0; i < cmd->arg_count; i++)
	//			arg = cmd->data.list[i];
	// command_process(arg1,arg2,...);

	return NULL;
}


DOTCONF_CB(default_callback)
{
	int i, arg_count;
	char *command, *arg ;

	printf("default_callback handler called for \"%s\". Got %d args\n", cmd->name,
	       cmd->arg_count);
	for (i = 0; i < cmd->arg_count; i++)
		printf("\tArg #%d: %s\n", i, cmd->data.list[i]);

	command = cmd->name;
	arg_count = cmd->arg_count;
	if ( strcmp(command,"command1")== 0 ) {
		// read all string as input argments
		for (i = 0; i < cmd->arg_count; i++)
			arg = cmd->data.list[i];
		// arg is a string , you can transfer it to int with atoi()
		// process command1
		// command1_process(arg1,arg2,...);
	}

	return NULL;
}

int gwconfig_test(void)
{
	char *filename = "/home/makui/projects/gw/src/common/dotconf/dotconf_template.conf";

	gwconfig(filename);
	return 0;
}

/*
  vim:set ts=4:
  vim:set shiftwidth=4:
*/
