/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* lbrdbdctl, A tool for sending commands to lbrdbd.
*/

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <assert.h>

#include "lb.h"
#include "rule_tree.h"
#include "rule_tree_rpc.h"
#include "mapping.h"
#include "liblb.h"

#include "exported.h"
#include "ldbox_version.h"
#include "liblbcallers.h"

void *liblb_handle = NULL;

/* create call_lb__ruletree_rpc__init2__() */
LIBLB_CALLER(char *, lb__ruletree_rpc__init2__,
	(void), (),
	NULL)

/* create call_lb__ruletree_rpc__ping__() */
LIBLB_VOID_CALLER(lb__ruletree_rpc__ping__,
	(void), ())

static const char *progname = NULL;
char    *ldbox_session_dir = NULL;

int ruletree_get_min_client_socket_fd(void)
{
	return(10);
}

int main(int argc, char *argv[])
{
	int		opt;
	const char	*cmd;
	const char	*loglevel = "warning";
	int		dlopen_liblb = 1;

	progname = argv[0];

	while ((opt = getopt(argc, argv, "l:s:n")) != -1) {
		switch (opt) {
		case 'l':
			loglevel = optarg;
			break;
		case 's':
                        ldbox_session_dir = strdup(optarg);
                        break;
		case 'n':
                        dlopen_liblb = 0;
                        break;
		default:
			fprintf(stderr, "Illegal option\n");
			exit(1);
		}
	}

	/* init logging.
	 * if debug_level and/or debug_file is NULL, logger
	 * will read the values from env.vars. */
	lblog_init_level_logfile_format(loglevel, "-", NULL);

	if (dlopen_liblb) {
		/* disable mapping; dlopen must run without mapping
		 * if running inside an lb session */
		setenv("LDBOX_DISABLE_MAPPING", "1", 1/*overwrite*/);
		liblb_handle = dlopen(LIBLB_SONAME, RTLD_NOW);
		unsetenv("LDBOX_DISABLE_MAPPING"); /* enable mapping */
	}

	if (!ldbox_session_dir) {
		ldbox_session_dir = getenv("LDBOX_SESSION_DIR");
	}
	if (!ldbox_session_dir) {
		fprintf(stderr, "ERROR: no session "
			"(LDBOX_SESSION_DIR is not set) - use option -s or "
			"run this program inside a Scratchbox 2 session\n");
		exit(1);
	}

	cmd = argv[optind];
	if (!cmd) {
		fprintf(stderr, "Usage:\n\t%s command\n", argv[0]);
		fprintf(stderr, "commands\n"
				"   ping     Send a 'ping' to lbrdbd\n"
				"   init2    Send a 'init2' to lbrdbd, wait and print the reply\n");
		exit(1);
	}

	if (!strcmp(cmd, "ping")) {
		if (liblb_handle) {
			call_lb__ruletree_rpc__ping__();
		} else {
			ruletree_rpc__ping();
		}
	} else if (!strcmp(cmd, "init2")) {
		char *msg;
		if (liblb_handle) {
			msg = call_lb__ruletree_rpc__init2__();
		} else {
			msg = ruletree_rpc__init2();
		}
		if (msg) {
			printf("%s\n", msg);
			free(msg);
		} else {
			exit(1);
		}
	} else {
		fprintf(stderr, "Unknown command %s\n", cmd);
		exit(1);
	}

	return(0);
}

/* This program is directly linked to the RPC routines
 * (because they are hidden in liblb, and could not be used otherwise).
 * The RPC routines want some wrappers for *_nomap_nolog functions,
 * these will use the ordinary functions... a side-effect is that the 
 * network addresses are subject to mapping operations here. 
 * Fortunately that won't happen in the usual case when the RPC
 * functions are used inside liblb.
*/
int unlink_nomap_nolog(const char *pathname)
{
	return(unlink(pathname));
}

ssize_t sendto_nomap_nolog(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	return(sendto(s, buf, len, flags, to, tolen));
}

ssize_t recvfrom_nomap_nolog(int s, void *buf, size_t len, int flags, __SOCKADDR_ARG from, socklen_t *fromlen)
{
	return(recvfrom(s, buf, len, flags, from, fromlen));
}

int bind_nomap_nolog(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen)
{
	return(bind(sockfd, my_addr, addrlen));
}

int chmod_nomap_nolog(const char *path, mode_t mode)
{
	return(chmod(path, mode));
}

