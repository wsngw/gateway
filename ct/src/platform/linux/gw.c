/*
 * gw.c
 *
 *  Created on: 2010-11-1
 *      Author: makui
 */

#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <module.h>
#include <dotconf/dotconf.h>

int responsehdr_test (void);
int test_http_post (void);
int jsonrpc_test(void);
int uart_test(void);
void uart_adaptor_test( void);
int context_test(void);
int wsnadpt_config(void);

static gw_t *gw;

static bool_t sig_caught = FALSE;
static bool_t opt_cf_set = FALSE;
static bool_t opt_pf_set = FALSE;
static bool_t opt_fg_set = FALSE;
static bool_t opt_lf_set = FALSE;
static bstring opt_cf, opt_pf, opt_lf;
static bool_t opt_fg = FALSE;

static DOTCONF_CB(default_callback);
static gw_t * gw_init(void);
static void gw_destroy(void);
static void free_modules(void);
static bool_t load_modules(UT_array *modules);

static const configoption_t options[] = {
	{"daemon", ARG_TOGGLE, default_callback, NULL, 0},
	{"", ARG_NAME, default_callback, NULL, 0},
	LAST_OPTION
};

DOTCONF_CB(default_callback)
{
	int i, arg_count;
	bstring command;
	bstring tmpcmd;

	//gw_debug_print("default_callback handler called for \"%s\". Got %d args\n",
	//		cmd->name, cmd->arg_count);

	//for (i = 0; i < cmd->arg_count; i++)
	//	gw_debug_print("\tArg #%d: %s\n", i, cmd->data.list[i]);

	command = bfromcstr(cmd->name);
	arg_count = cmd->arg_count;

	tmpcmd = bfromcstr("NUI");
	if (bstricmp(command, tmpcmd) == 0)
	{
		bassigncstr(gw->config->nui, cmd->data.list[0]);
		gw_debug_print("the nui is %s\n", gw->config->nui->data);
	}

	bassigncstr(tmpcmd, "loadmodule");
	if (bstricmp(command, tmpcmd) == 0)
	{
		utarray_push_back(gw->config->module, &cmd->data.list[0]);
		gw_debug_print("load module %s\n",
				*((char **) utarray_back(gw->config->module)));
	}

	bassigncstr(tmpcmd, "daemon");
	if (bstricmp(command, tmpcmd) == 0)
	{
		gw->config->daemon = cmd->data.value;
		gw_debug_print("daemon value is set to %d\n", gw->config->daemon);
	}

	bassigncstr(tmpcmd, "logpath");
	if (bstricmp(command, tmpcmd) == 0)
	{
		bassigncstr(gw->config->logpath, cmd->data.list[0]);
		gw_debug_print("the log path is %s\n", gw->config->logpath->data);
	}

	bassigncstr(tmpcmd, "pidpath");
	if (bstricmp(command, tmpcmd) == 0)
	{
		bassigncstr(gw->config->pidpath, cmd->data.list[0]);
		gw_debug_print("the pid path is %s\n", gw->config->pidpath->data);
	}

	bdestroy(command);
	bdestroy(tmpcmd);

	return NULL;
}


static void daemonize()
{
	pid_t pid;
	if ((pid = fork()) == -1)
	{
		printf ("fork error\n");
		exit (-1);
	}
	else if (pid > 0)
	{
		printf ("Running on background\n");
		printf ("PID : %d\n", pid);
		exit (0);
	}

	close (fileno (stdin));
	close (fileno (stdout));
	close (fileno (stderr));

	setsid();
	chdir("/");
	umask(0);
}

static void cleanup(void)
{
	gw_debug_print("cleanup called\n");
	gw_destroy();
}

static void sig_handler(int arg)
{
//	gw_log_print(GW_LOG_LEVEL_INFO, "SIGTERM or SIGINT received\n");
	gw_debug_print("SIGTERM or SIGINT received\n");
	if (!sig_caught)
	{
		sig_caught = TRUE;
		exit(0);
	}
//	gw_log_print(GW_LOG_LEVEL_INFO, "after exit(0)\n");
}

static void sigsegv_dump(int signo)
{
	uint8_t buf[2048] ;
	uint8_t cmd[2048];
	FILE *fh;

	snprintf(buf, sizeof(buf), "/proc/%d/cmdline",getpid());
	if(!(fh = fopen(buf,"r")))
		exit(0);
	if(!fgets(buf,sizeof(buf),fh))
		exit(0);
	fclose(fh);

	if(buf[strlen(buf)-1]=='\n')
		buf[strlen(buf)-1]='\0';
	snprintf(cmd,sizeof(cmd),"gdb %s %d",buf,getpid());
	printf("SIGSEGV segment fault captured by sigsegv_dump()!\n");
	system(cmd);

	exit(0);
}

int dummy_function(void)
{
	char buf[20];
	char *p;
	char *psys = 0x00;  /*point to system memery*/

	p = buf;
	*(p+200)=0x00;  /*out of space, generate SIGSEGV signal*/

	*psys = 0x00;   /*access system memory space, generat SIGSEGV */

	return 1;
}

static gw_t * gw_init(void)
{
	gw_t *gw = calloc(1, sizeof(gw_t));

	gw_log_open(GW_LOG_BACKEND_STDIO, NULL);
//	gw_log_open(GW_LOG_BACKEND_FILE, "/home/makui/gw.log");
	gw_log_set_level(GW_LOG_LEVEL_CRITICAL);

	gw_infobase_init();
	dev_infobase_init();

	gw->data = gw_data_get();
	gw->sn = sn_data_head();
	gw->config = gw_config_get();

	opt_cf = bfromcstr("");
	opt_lf = bfromcstr("");
	opt_pf = bfromcstr("");

	mod_sys_init(gw);
	gw_msg_init();

	return gw;
}

static void gw_destroy(void)
{
	gw_debug_print("gw_destroy called\n");

	free_modules();
	gw_msg_destroy();
	mod_sys_destroy(gw);

	bdestroy(opt_cf);
	bdestroy(opt_lf);
	bdestroy(opt_pf);

	gw_infobase_destroy();
	dev_infobase_destroy();
	gw_log_close();
	free(gw);
}

static bool_t load_modules(UT_array *modules)
{
	bool_t ret = TRUE;
	char **name = NULL;
	//bstring module = bfromcstr("plugins/");
	bstring module = bfromcstr("");

	while (name = (char**)utarray_next(modules, name))
	{
		//bcatcstr(module, *name);
		//bassigncstr(module, "/home/makui/projects/gw/bin/i386-Linux/");
		//bcatcstr(module, *name);
		bassigncstr(module, *name);
		bcatcstr(module, ".so");
		if (!mod_load(gw, module->data))
		{
			ret = FALSE;
			gw_log_print(GW_LOG_LEVEL_CRITICAL, "Cannot find module %s\n", *name);
		}
	}

	bdestroy(module);

	return ret;
}

static void free_modules(void)
{
	gw_mod_t *next = NULL;
	gw_mod_t *curr = NULL;

	while (curr = mod_get_next_safe(gw, curr, &next))
	{
		mod_free(gw, curr);
		curr = next;
	}
}

static bool_t load_config()
{
	bool_t ret = FALSE;
	configfile_t *configfile;

	if (opt_cf_set)
		bassign(gw->config->cfgpath, opt_cf);

	configfile = dotconf_create(gw->config->cfgpath->data, options, NULL, CASE_INSENSITIVE);

	if (!configfile)
	{
		fprintf(stderr, "Error creating configfile\n");
		return ret;
	}

	if (dotconf_command_loop(configfile) == 0)
	{
		fprintf(stderr, "Error parsing config file %s\n", configfile->filename);
		return ret;
	}

	dotconf_cleanup(configfile);

	if (opt_fg_set) gw->config->daemon = !opt_fg;
	if (opt_lf_set) bassign(gw->config->logpath, opt_lf);

	ret = TRUE;

	return ret;
}

static void show_version(void)
{
	printf("gw version 1.0\n");
}

static void show_help(void)
{
	char *help =
			"gw: a sensor network gateway\r\n" \
			"Usage: gw [OPTION]...\r\n" \
			"  -g, --config=config file path         Set config file path\n" \
			"  -p, --pid=pid file path               Set pid file path\n" \
			"  -f, --foreground                      Running on foreground\n" \
			"  -l, --log=log file path               Set log file path\n" \
			"  -v, --version                         Print version information and exit\n" \
			"  -h, --help                            Print this help information and exit\n";

	write(STDOUT_FILENO, help, strlen(help));
}

static const char *shortopts = "hg:p:fl:v";
struct option longopts[] = {
	{"help", no_argument, NULL, 'h'},
	{"config", required_argument, NULL, 'g'},
	{"pid", required_argument, NULL, 'p'},
	{"foreground", no_argument, NULL, 'f'},
	{"log", required_argument, NULL, 'l'},
	{"version", no_argument, NULL, 'v'},
	{0, 0, 0, 0},
};

int main(int argc, char **argv)
{
	sigset_t sigset, pendset;
	struct sigaction action;

	int c;

	/** -----test program added by wanghl,pls keep it------ */
		//daemonize();
		//gwconfig_test();
	    //responsehdr_test();
		// test_http_post();
		//uart_test();
//		uart_adaptor_test();
//	context_test();
//	wsnadpt_config();


	signal(SIGSEGV, &sigsegv_dump); /*locate SIGSEGV code for debug*/

	gw = gw_init();
	atexit(cleanup);

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1)
	{
		switch (c)
		{
		case 'g':
			opt_cf_set = TRUE;
			bassigncstr(opt_cf, optarg);
			break;
		case 'p':
			opt_pf_set = TRUE;
			bassigncstr(opt_pf, optarg);
			break;
		case 'f':
			opt_fg_set = TRUE;
			opt_fg = TRUE;
			break;
		case 'l':
			opt_lf_set = TRUE;
			bassigncstr(opt_lf, optarg);
			break;
		case 'v':
			show_version();
			return 0;
		case 'h':
		case '?':
		default:
			show_help();
			return 0;
		}
	}

	load_config();

	if (gw->config->daemon)
	{
		daemonize();
	}

	load_modules(gw->config->module);

//	dummy_function(); /*test, generat SIGSEGV segment fault*/

	sigemptyset(&action.sa_mask);
	action.sa_handler = sig_handler;
	sigaction(SIGTERM, &action, NULL);

	sigemptyset(&action.sa_mask);
	action.sa_handler = sig_handler;
	sigaction(SIGINT, &action, NULL);

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGHUP);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	while (1)
	{
		sigemptyset(&pendset);
		sigpending(&pendset);

		usleep(1000 * 100);

		if (sigismember(&pendset, SIGHUP)) {
			sigemptyset(&action.sa_mask);
			action.sa_handler = SIG_IGN;
			sigaction(SIGHUP, &action, NULL);
			gw_log_print(GW_LOG_LEVEL_INFO, "SIGHUP received, reload config now\n");
			//loadconfig();
			action.sa_handler = SIG_DFL;
			sigaction(SIGHUP, &action, NULL);
		}
	}

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGHUP);
	sigprocmask(SIG_UNBLOCK, &sigset, NULL);

	return 0;
}
