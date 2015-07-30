/* Error + trace + debug logging routines for LB.
 *
 * Copyright (C) 2007 Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

/* Two log formats are supported:
 * 1) Normal (default) log entry consists of three or four tab-separated
 *    fields:
 *     - Timestamp and log level
 *     - Process name ("binaryname") and process id (number)
 *     - The log message. If the original message contains tabs and/or
 *       newlines, those will be filtered out.
 *     - Optionally, source file name and line number.
 * 2) Simple format minimizes varying information and makes it easier to 
 *    compare log files from different runs. Fields are:
 *     - Log level
 *     - Process name
 *     - The message
 *     - Optionally, source file name and line number.
 *    Simple format can be enabled by setting environment variable 
 *    "LDBOX_MAPPING_LOGFORMAT" to "simple".
 *
 * Note that logfiles are used by at least two other components
 * of ldbox: lb-monitor/lb-exitreport notices if errors or warnings have
 * been generated during the session, and lb-logz can be used to generate
 * summaries.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>

#include <lb.h>
#include <config.h>

#include "exported.h"
#include "ldbox_version.h"

/* -------- Config: buffer sizes: */

#define LOG_TIMESTAMP_BUFSIZE 24

/* max.len. of the log message, including \0 but not including line headers */
#define LOG_MSG_MAXLEN	500

/* max.len. of source file name + line number field */
#define LOG_SRCLOCATION_MAXLEN	150

#define LOG_LEVELNAME_MAXLEN 10

#define LOG_BINARYNAME_MAXLEN 80

#define LOG_PIDANDTID_MAXLEN 80

/* max. size of one line:
 * timestamp (levelname)\tbinaryname,process_and_thread_id\tlogmsg,srclocation */
#define LOG_LINE_BUFSIZE (LOG_TIMESTAMP_BUFSIZE+2+LOG_LEVELNAME_MAXLEN+2+ \
			  LOG_BINARYNAME_MAXLEN+LOG_PIDANDTID_MAXLEN+1+ \
			  LOG_MSG_MAXLEN+LOG_SRCLOCATION_MAXLEN)

#define LOGFILE_NAME_BUFSIZE 512

/* ===================== Internal state variables =====================
 *
 * N.B. no mutex protecting concurrent writing to these variables.
 * currently a mutex is not needed, because all of these are
 * written only at startup and values come from environment variables...
*/

static struct lb_log_state_s {
	int		lbl_print_file_and_line;
	int		lbl_simple_format;
	char		lbl_binary_name[LOG_BINARYNAME_MAXLEN];
	char		lbl_logfile[LOGFILE_NAME_BUFSIZE];
} lb_log_state = {
	.lbl_print_file_and_line = 0,
	.lbl_simple_format = 0,
	.lbl_binary_name = {0},
	.lbl_logfile = {0},
};

/* ===================== public variables ===================== */

/* loglevel needs to be public, it is used from the logging macros */
int lb_loglevel__ = LB_LOGLEVEL_uninitialized;

int lb_log_initial_pid__ = 0;

/* ===================== private functions ===================== */

/* create a timestamp in format "YYYY-MM-DD HH:MM:SS.sss", where "sss"
 * is the decimal part of current second (milliseconds).
*/
static void make_log_timestamp(char *buf, size_t bufsize)
{
	struct timeval	now;

	if (gettimeofday(&now, (struct timezone *)NULL) < 0) {
		*buf = '\0';
		return;
	}
	/* localtime_r() or gmtime_r() can cause deadlocks
	 * inside glibc (if this logger is called via a signal
	 * handler), so can't convert the time to a more
	 * user-friedly format. Sad. */
	snprintf(buf, bufsize, "%u.%03u",
		(unsigned int)now.tv_sec, (unsigned int)(now.tv_usec/1000));
}

/* Write a message block to a logfile.
 *
 * Note that a safer but slower design was selected intentionally:
 * a routine which always opens a file, writes the message and then closes
 * the file is really stupid if only performance is considered. Usually
 * anyone would open the log file only once and keep it open, but here
 * a different strategy was selected because this library should be transparent
 * to the running program and having an extra open fd hanging around might
 * introduce really peculiar problems with some programs.
*/
static void write_to_logfile(const char *msg, int msglen)
{
	int logfd;

	if (lb_log_state.lbl_logfile[0]) {
		if (lb_log_state.lbl_logfile[0] == '-' &&
		    lb_log_state.lbl_logfile[1] == '\0') {
			/* log to stdout. */
			int r; /* needed to get around some unnecessary warnings from gcc*/
			r = write(1, msg, msglen);
			(void)r;
		} else if ((logfd = open_nomap_nolog(lb_log_state.lbl_logfile,
					O_APPEND | O_RDWR | O_CREAT,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
					| S_IROTH | S_IWOTH)) >= 0) {
			int r; /* needed to get around some unnecessary warnings from gcc*/
			r = write(logfd, msg, msglen);
			(void)r;
			close_nomap_nolog(logfd);
		}
	}
}

/* ===================== public functions ===================== */

int lblog_level_name_to_number(const char *level_str)
{
	int level;

	if (!level_str) return(0);

	/* Both logfile and loglevel have been set. */
	if (!strcmp(level_str,"error")) {
		level = LB_LOGLEVEL_ERROR;
	} else if (!strcmp(level_str,"warning")) {
		level = LB_LOGLEVEL_WARNING;
	} else if (!strcmp(level_str,"net")) {
		level = LB_LOGLEVEL_NETWORK;
	} else if (!strcmp(level_str,"notice")) {
		level = LB_LOGLEVEL_NOTICE;
	} else if (!strcmp(level_str,"info")) {
		level = LB_LOGLEVEL_INFO;
	} else if (!strcmp(level_str,"debug")) {
		level = LB_LOGLEVEL_DEBUG;
	} else if (!strcmp(level_str,"noise")) {
		level = LB_LOGLEVEL_NOISE;
	} else if (!strcmp(level_str,"noise2")) {
		level = LB_LOGLEVEL_NOISE2;
	} else if (!strcmp(level_str,"noise3")) {
		level = LB_LOGLEVEL_NOISE3;
	} else {
		level = LB_LOGLEVEL_INFO;
	}
	return(level);
}

void lblog_init_level_logfile_format(const char *opt_level, const char *opt_logfile, const char *opt_format)
{
	if (lb_loglevel__ == LB_LOGLEVEL_uninitialized) {
		const char	*level_str;
		const char	*format_str;
		const char	*filename;

		if (!lb_global_vars_initialized__) {
			lb_initialize_global_variables();
			if (lb_global_vars_initialized__) {
				/* All ok. lb_initialize_global_variables()
				 * called us, no need to continue here.
				*/
				return;
			}
		}

		if (ldbox_exec_name &&
		    ldbox_orig_binary_name &&
		    strcmp(ldbox_exec_name, ldbox_orig_binary_name)) {
			/* this is an interpreter, running a script.
			 * set .lbl_binary_name to 
			 * scriptbasename{interpreterbasename}
			*/
			char *cp;

			if ((cp=strrchr(ldbox_exec_name, '/')) != NULL) {
				cp++;
			} else {
				cp = ldbox_exec_name;
			}
			snprintf(lb_log_state.lbl_binary_name, sizeof(lb_log_state.lbl_binary_name),
				"%s{%s}", cp, ldbox_binary_name);
		} else {
			snprintf(lb_log_state.lbl_binary_name, sizeof(lb_log_state.lbl_binary_name),
				"%s", ldbox_binary_name ? ldbox_binary_name : "");
		}

		filename = (opt_logfile ? opt_logfile : getenv("LDBOX_MAPPING_LOGFILE"));
		if (filename)
			snprintf(lb_log_state.lbl_logfile, sizeof(lb_log_state.lbl_logfile),
				"%s", (opt_logfile ? opt_logfile : getenv("LDBOX_MAPPING_LOGFILE")));
		else
			lb_log_state.lbl_logfile[0] = '\0';

		level_str = opt_level ? opt_level : getenv("LDBOX_MAPPING_LOGLEVEL");
		if (lb_log_state.lbl_logfile[0]) {
			if (level_str) {
				/* Both logfile and loglevel have been set. */
				lb_loglevel__ = lblog_level_name_to_number(level_str);
				if (lb_loglevel__ >= LB_LOGLEVEL_DEBUG)
					lb_log_state.lbl_print_file_and_line = 1;
			} else {
				/* logfile set, no level specified: */
				lb_loglevel__ = LB_LOGLEVEL_INFO;
			}
		} else {
			/* no logfile, don't log anything. */
			lb_loglevel__ = LB_LOGLEVEL_NONE;
		}

		format_str = opt_format ? opt_format : getenv("LDBOX_MAPPING_LOGFORMAT");
		if (format_str) {
			if (!strcmp(format_str,"simple")) {
				lb_log_state.lbl_simple_format = 1;
			}
		}

		/* initialized, write a mark to logfile. */
		/* NOTE: Following LB_LOG() call is used by the log
		 *       postprocessor script "lblogz". Do not change
		 *       without making a corresponding change to the script!
		*/
		LB_LOG(LB_LOGLEVEL_INFO,
			 "---------- Starting (" LDBOX_VERSION ")"
			" [] "
			"ppid=%d <%s> (%s) ----------%s",
			getppid(),
			ldbox_exec_name ?
				ldbox_exec_name : "",
			ldbox_active_exec_policy_name ?
				ldbox_active_exec_policy_name : "",
			ldbox_mapping_method ?
				ldbox_mapping_method : "");

		lb_log_initial_pid__ = getpid();
	}
}

void lblog_init(void)
{
	lblog_init_level_logfile_format(NULL,NULL,NULL);
}

/* a vprintf-like routine for logging. This will
 * - prefix the line with current timestamp, log level of the message, and PID
 * - add a newline, if the message does not already end to a newline.
*/
void lblog_vprintf_line_to_logfile(
	const char	*file,
	int		line,
	int		level,
	const char	*format,
	va_list		ap)
{
	char	tstamp[LOG_TIMESTAMP_BUFSIZE];
	char	logmsg[LOG_MSG_MAXLEN];
	char	finalmsg[LOG_LINE_BUFSIZE];
	int	msglen;
	char	*forbidden_chrp;
	char	optional_src_location[LOG_SRCLOCATION_MAXLEN];
	char	*levelname = NULL;

	if (lb_loglevel__ == LB_LOGLEVEL_uninitialized) lblog_init();

	if (lb_log_state.lbl_simple_format) {
		*tstamp = '\0';
	} else {
		/* first, the timestamp: */
		if (level > LB_LOGLEVEL_WARNING) {
			/* no timestamps to errors & warnings */
			make_log_timestamp(tstamp, sizeof(tstamp));
		} else {
			*tstamp = '\0';
		}
	}

	/* next, print the log message to a buffer: */
	msglen = vsnprintf(logmsg, sizeof(logmsg), format, ap);

	if (msglen < 0) {
		/* OOPS. should log an error message, but this is the
		 * logger... can't do it */
		logmsg[0] = '\0';
	} else if (msglen > (int)sizeof(logmsg)) {
		/* message was truncated. logmsg[LOG_MSG_MAXLEN-1] is '\0' */
		logmsg[LOG_MSG_MAXLEN-3] = logmsg[LOG_MSG_MAXLEN-2] = '.';
	}

	/* post-format the log message.
	 *
	 * First, clean all newlines: some people like to use \n chars in
	 * messages, but that is forbidden (attempt to manually reformat
	 * log messages *will* break all post-processing tools).
	 * - remove trailing newline(s)
	 * - replace embedded newlines by $
	 *
	 * Second, replace all tabs by spaces because of similar reasons
	 * as above. We'll use tabs to separate the pre-defined fields below.
	*/
	while ((msglen > 0) && (logmsg[msglen-1] == '\n')) {
		logmsg[msglen--] = '\0';
	}
	while ((forbidden_chrp = strchr(logmsg,'\n')) != NULL) {
		*forbidden_chrp = '$'; /* newlines to $ */
	}
	while ((forbidden_chrp = strchr(logmsg,'\t')) != NULL) {
		*forbidden_chrp = ' '; /* tabs to spaces */
	}

	/* combine the timestamp and log message to another buffer.
	 * here we use tabs to separate fields. Note that the location,
	 * if present, should always be the last field (so that same
	 * post-processing tools can be used in both cases)  */
	if (lb_log_state.lbl_print_file_and_line) {
		snprintf(optional_src_location, sizeof(optional_src_location), "\t[%s:%d]", file, line);
	} else {
		optional_src_location[0] = '\0';
	}

	switch(level) {
	case LB_LOGLEVEL_ERROR:		levelname = "ERROR"; break;
	case LB_LOGLEVEL_WARNING:	levelname = "WARNING"; break;
	case LB_LOGLEVEL_NETWORK:	levelname = "NET"; break;
	case LB_LOGLEVEL_NOTICE:	levelname = "NOTICE"; break;
	/* default is to pass level info as numbers */
	}
	
	if (lb_log_state.lbl_simple_format) {
		/* simple format. No timestamp or pid, this makes
		 * it easier to compare logfiles.
		*/
		if(levelname) {
			snprintf(finalmsg, sizeof(finalmsg), "(%s)\t%s\t%s%s\n",
				levelname, lb_log_state.lbl_binary_name,
				logmsg, optional_src_location);
		} else {
			snprintf(finalmsg, sizeof(finalmsg), "(%d)\t%s\t%s%s\n",
				level, lb_log_state.lbl_binary_name,
				logmsg, optional_src_location);
		}
	} else {
		char	process_and_thread_id[LOG_PIDANDTID_MAXLEN];

		if (pthread_library_is_available && pthread_self_fnptr) {
			pthread_t	tid = (*pthread_self_fnptr)();

			snprintf(process_and_thread_id, sizeof(process_and_thread_id),
				"[%d/%ld]", getpid(), (long)tid);
		} else {
			snprintf(process_and_thread_id, sizeof(process_and_thread_id),
				"[%d]", getpid());
		}

		/* full format */
		if(levelname) {
			snprintf(finalmsg, sizeof(finalmsg), "%s (%s)\t%s%s\t%s%s\n",
				tstamp, levelname, lb_log_state.lbl_binary_name,
				process_and_thread_id, logmsg,
				optional_src_location);
		} else {
			snprintf(finalmsg, sizeof(finalmsg), "%s (%d)\t%s%s\t%s%s\n",
				tstamp, level, lb_log_state.lbl_binary_name,
				process_and_thread_id, logmsg,
				optional_src_location);
		}
	}

	write_to_logfile(finalmsg, strlen(finalmsg));
}

void lblog_printf_line_to_logfile(
	const char	*file,
	int		line,
	int		level,
	const char	*format,
	...)
{
	va_list	ap;

	if (lb_loglevel__ == LB_LOGLEVEL_uninitialized) lblog_init();

	va_start(ap, format);
	lblog_vprintf_line_to_logfile(file, line, level, format, ap);
	va_end(ap);
}
