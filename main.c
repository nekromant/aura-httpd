#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>
#include <getopt.h>
#include <libdaemon/daemon.h>
#include <signal.h>

int do_daemonize = 0;
int do_kill = 0;

enum daemon_error_codes {
	DAEMON_FAIL_TO_CLOSE,
	DAEMON_FAIL_PID,
	DAEMON_FAIL_CONF,
	DAEMON_FAIL_START,
};

const char *daemon_error_messages[] = {
	"Daemon failed to close all useless descriptors",
	"Daemon failed to create PID file (Are you root?)",
	"Daemon failed to read and parse configuration",
	"Daemon failed to start the server",
};


static const struct option long_options[] = {
	{ "daemonize", no_argument,	  		0, 			'd' },
	{ "kill",      no_argument,	  		0,	 		'k' },
	{ "help",      no_argument,	  		0,		 	'h' },
	{ "debug",     required_argument, 	0,		 	'D' },
	{ "config",    required_argument, 	0,		 	'c' },
	{ "logfile",   required_argument, 	0,		 	'l' },
	{ "serverid",  required_argument, 	0,		 	'i' },
	{}
};


const char helpstr[] =
"Aura HTTP Daemon | (c) Andrew 'Necromant' Andrianov 2016\n"
"This is free software; see the source for copying conditions.  There is NO\n"
"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n"
"Usage: %s [options]\n"
"Valid options are: \n\n"
"--version   | -v           - Print out version info\n"
"--help      | -h           - Show this help\n"
"--config    | -c path      - Path to server configuration. Default: config.json\n"
"--debug     | -d N         - Set debugging/logging level to N\n"
"--logfile   | -l path      - Log to this file (default - stdout)\n"
"--daemonize | -d           - Run as a system daemon\n"
"--kill      | -k           - Kill a running daemon\n"
"                             Bigger numbers produce more noise\n"
"--serverid  | -i string    - Set server identifier (pidfile, logging) to string\n"
"                             There can be only one aurahttpd daemon with\n"
"                             the same server id running. Default: aurahttpd\n"
;
void usage(const char *argv0)
{
	fprintf(stderr, helpstr, argv0);
	exit(1);
}

void version()
{
	printf("libaura version %s (numeric %u)\n",
		   aura_get_version(), aura_get_version_code());
	exit(1);
}

int permissions_downgrade(const char *user, const char *pass)
{
	/* TODO */
	return 0;
};

int chroot(const char *path)
{
	/* TODO */
	return 0;
}

int main(int argc, char *argv[])
{
	int loglevel = 0;
	pid_t pid = 0;
	const char *configfile = "config.json";
	const char *logfile = NULL;
	const char *serverid = "aurahttpd";

	int option_index = 0;
	int c;

	while (1) {
		c = getopt_long(argc, argv, "dvkhD:c:l:",
				long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'c':
			configfile = optarg;
			break;
		case 'D':
			loglevel = atoi(optarg);
			break;
		case 'l':
			logfile = optarg;
			break;
		case 'i':
			serverid = optarg;
			break;
		case 'd':
			do_daemonize++;
			break;
		case 'k':
			do_kill++;
			break;
		case 'h':
			usage(argv[0]);
			break;
		case 'v':
			version();
			break;

		}
	}

	configfile = realpath(configfile, NULL);
	slog_init(NULL, loglevel);

	daemon_pid_file_ident = daemon_log_ident = serverid;
	slog(0, SLOG_DEBUG, "Server identifier: %s", daemon_pid_file_ident);

	pid = daemon_pid_file_is_running();

	if (do_kill) {
		if (!(pid > 0)) {
			slog(0, SLOG_INFO, "No daemon process running, nobody to kill");
			return 0;
		}

		slog(0, SLOG_LIVE, "Sending SIGINT to daemon process");
		if (daemon_pid_file_kill_wait(SIGINT, 30)) {
			slog(0, SLOG_LIVE, "Daemon didn't exit, sending SIGKILL...");
			if (daemon_pid_file_kill_wait(SIGKILL, 5)) {
				slog(0, SLOG_LIVE, "That didn't help, please deal with pid %u yourself", pid);
				return 1;
			}
		}

		return 0;
	}

	if (pid >= 0) {
		slog(0, SLOG_ERROR, "AuraHTTPD is already running with PID %u", pid);
		return 1;
	}

	slog(0, SLOG_DEBUG, "%d\n", do_daemonize);
	/* Do the fork, if needed */
	if (do_daemonize) {
		/* Prepare for return value passing from the initialization procedure of the daemon process */
		if (daemon_retval_init() < 0) {
			slog(0, SLOG_ERROR, "Failed to create pipe.");
			return 1;
		}
		pid = daemon_fork();
	} else
		pid = 0;

	if (pid < 0) {
		slog(0, SLOG_ERROR, "Failed to daemonize ;(");
		daemon_retval_done();
		return 1;
	} else if (pid) {
		int ret;
		/* Wait for 20 seconds for the return value passed from the daemon process */
		if ((ret = daemon_retval_wait(5)) < 0) {
			slog(0, SLOG_ERROR, "Could not receive return value from daemon process: %s", strerror(errno));
			return 255;
		}
		if (!ret)
			slog(0, SLOG_LIVE, "AuraHTTPD is now up and running!");
		else
			slog(0, SLOG_ERROR, "Failed to start AuraHTTPD");
			slog(0, SLOG_ERROR, daemon_error_messages[ret]);
		return 0;
	}

	if (do_daemonize) {
		/* At this point we're definetely the daemon if we're daemonizing */
		/* So let's do our daemonic thingies here */
		if (daemon_close_all(-1) < 0) {
			slog(0, SLOG_ERROR, "Failed to close all file descriptors: %s", strerror(errno));
			daemon_retval_send(DAEMON_FAIL_TO_CLOSE);
			goto bailout;
		}

		if (daemon_pid_file_create() < 0) {
		  	slog(0, SLOG_ERROR, "Could not create PID file (%s).", strerror(errno));
		  	daemon_retval_send(DAEMON_FAIL_PID);
		  	goto bailout;
	  	}

	}
	/* Reinit slog to log to file*/
	slog_init(logfile, loglevel);
	struct json_object *conf = json_load_from_file(configfile);
	if (!conf) {
		slog(0, SLOG_ERROR, "Error loading configuration: %s", configfile);
		daemon_retval_send(DAEMON_FAIL_CONF);
		goto bailout;
	}

	struct ahttpd_server *server = ahttpd_server_create(conf);
	if (!server) {
		slog(0, SLOG_ERROR, "Error loading configuration: %s", configfile);
		daemon_retval_send(DAEMON_FAIL_START);
		goto bailout;
	}

	if (do_daemonize)
		daemon_retval_send(0);

	json_object_put(conf);
	aura_eventloop_dispatch(server->aloop, 0);
	ahttpd_server_destroy(server);

bailout:
	slog(0, SLOG_LIVE, "Exiting...");
	daemon_pid_file_remove();
	return 0;
}
