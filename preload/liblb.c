/*
 * liblb -- initalization, utility functions, etc of
 *           the ldbox preload library
 *
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * parts contributed by 
 * 	Riku Voipio <riku.voipio@movial.com>
 *	Toni Timonen <toni.timonen@movial.com>
 *	Lauri T. Aarnio
 * 
 * Heavily based on the libfakechroot library by 
 * Piotr Roszatycki <dexter@debian.org>
 */

/*
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <stdio.h>
#include <unistd.h>
#include <config.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include "liblb.h"
#include "exported.h"

/* String vector contents to a single string for logging.
 * returns pointer to an allocated buffer, caller should free() it.
*/
char *strvec_to_string(char *const *argv)
{
	int	result_max_size = 1;
	char	*const *vp;
	char	*buf;

	if (!argv) return(NULL);

	/* first, count max. size of the result */
	for (vp = argv; *vp; vp++) {
		/* add size of the string + one for the space + two for 
		 * the quotes (if needed) */
		result_max_size += strlen(*vp) + 3;
	}

	buf = malloc(result_max_size);
	*buf = '\0';

	/* next round: copy strings, using quotes for all strings that
	 * contain whitespaces. */
	for (vp = argv; *vp; vp++) {
		int	needs_quotes = 0;
		char	*cp = *vp;

		if (*cp) {
			/* non-empty string; see if it contains whitespace */
			while (*cp) {
				if (isspace(*cp)) {
					needs_quotes = 1;
					break;
				}
				cp++;
			}
		} else {
			/* empty string */
			needs_quotes = 1;
		}
		if (*buf) strcat(buf, " ");
		if (needs_quotes) strcat(buf, "'");
		strcat(buf, *vp);
		if (needs_quotes) strcat(buf, "'");
	}

	return(buf);
}

/* FIXME: The following two functions do not have anything to do with path
 * remapping. Instead these implementations prevent locking of the shadow
 * file, which I find really hard to understand. Please explain
 * why we should have these wrappers, or should these be removed completely?
 * (this same comment is in interface.master, so hopefully somebody
 * will be able to explain this someday!)
*/
/* #include <shadow.h> */
int lckpwdf (void)
{
	return 0;
}
int ulckpwdf (void)
{
	return 0;
}

/* ---------- */

void *ldbox_find_next_symbol(int log_enabled, const char *fn_name)
{
	char	*msg;
	void	*fn_ptr;

	if(log_enabled)
		LB_LOG(LB_LOGLEVEL_DEBUG, "%s: %s", __func__, fn_name);

	fn_ptr = dlsym(RTLD_NEXT, fn_name);

	if ((msg = dlerror()) != NULL) {
		fprintf(stderr, "%s: dlsym(%s): %s\n",
			PACKAGE_NAME, fn_name, msg);
		if(log_enabled)
			LB_LOG(LB_LOGLEVEL_ERROR, "ERROR: %s: dlsym(%s): %s",
				PACKAGE_NAME, fn_name, msg);
		assert(0);
	}
	if (log_enabled && LB_LOG_IS_ACTIVE(LB_LOGLEVEL_NOISE)) {
		Dl_info	dli;

		if (dladdr(fn_ptr, &dli)) {
			LB_LOG(LB_LOGLEVEL_NOISE, "%s: %s at 0x%p, in file '%s'",
				__func__, fn_name, fn_ptr, dli.dli_fname);
		} else {
			LB_LOG(LB_LOGLEVEL_NOISE, "%s: %s at 0x%p",
				__func__, fn_name, fn_ptr);
		}
	}

	return(fn_ptr);
}

/* ----- EXPORTED from interface.master: ----- */
char *lbshow__map_path2__(const char *binary_name, const char *mapping_mode,
        const char *fn_name, const char *pathname, int *readonly)
{
	char *mapped__pathname = NULL;
	mapping_results_t mapping_result;

	(void)mapping_mode;	/* mapping_mode is not used anymore. */

	if (!lb_global_vars_initialized__) lb_initialize_global_variables();

	clear_mapping_results_struct(&mapping_result);
	if (pathname != NULL) {
		ldbox_map_path_for_lbshow(binary_name, fn_name,
			pathname, &mapping_result);
		if (mapping_result.mres_result_path)
			mapped__pathname =
				strdup(mapping_result.mres_result_path);
		if (readonly) *readonly = mapping_result.mres_readonly;
	}
	free_mapping_results(&mapping_result);
	LB_LOG(LB_LOGLEVEL_DEBUG, "%s '%s'", __func__, pathname);
	return(mapped__pathname);
}

char *lbshow__reverse_path__(const char *func_name, const char *abs_path, uint32_t classmask)
{
	char *reversed__path = NULL;

	reversed__path = scratchbox_reverse_path(
		func_name, abs_path, classmask);
	LB_LOG(LB_LOGLEVEL_DEBUG, "%s '%s' => '%s'", __func__,
		abs_path, reversed__path ? reversed__path : "<none>");
	return(reversed__path);
}

char *lbshow__get_real_cwd__(const char *binary_name, const char *fn_name)
{
	char path[PATH_MAX];

	(void)binary_name;
	(void)fn_name;

	if (getcwd_nomap_nolog(path, sizeof(path))) {
		return(strdup(path));
	}
	return(NULL);
}

/* ---- support functions for the generated interface: */

/* returns true, if the "mode" parameter of fopen() (+friends)
 * indicates that write permission is required.
*/
int fopen_mode_w_perm(const char *mode)
{
	/* We'll have to look at the first characters only, since
	 * standard C allows implementation-specific additions to follow
	 * after the mode. "w", "a", "r+" and "rb+" need write access,
	 * but "r" or "r,access=lock" indicate read only access.
	 * For more information, see H&S 5th ed. p. 368
	*/
	if (*mode == 'w' || *mode == 'a') return (1);
	mode++;
	if (*mode == 'b') mode++;
	if (*mode == '+') return (1);
	return (0);
}

/* a helper function for freopen*(), which need to close the original stream
 * even if the open fails (e.g. this gets called when trying to open a R/O
 * mapped destination for R/W) */
int freopen_errno(FILE *stream)
{
	if (stream) fclose(stream);
	return (EROFS);
}


/* global variables, these come from the environment:
 * used to store session directory & ld.so settings.
 * set up as early as possible, so that if the application clears our
 * environment we'll still know the original values.
*/
char *ldbox_session_dir = NULL;
char *ldbox_session_mode = NULL; /* optional */
char *ldbox_vperm_ids = NULL;
char *ldbox_network_mode = NULL; /* optional */
char *ldbox_binary_name = NULL;
char *ldbox_exec_name = NULL;
char *ldbox_real_binary_name = NULL;
char *ldbox_orig_binary_name = NULL;
char *ldbox_active_exec_policy_name = NULL;
char *ldbox_mapping_method = NULL; /* optional */
char *ldbox_chroot_path = NULL; /* optional */

int lb_global_vars_initialized__ = 0;

/* to be used from lb-show only: */
void lb__set_active_exec_policy_name__(const char *name)
{
	LB_LOG(LB_LOGLEVEL_DEBUG, "lb__set_active_exec_policy_name__(%s)",
		name ? name : "NULL");
	ldbox_active_exec_policy_name = name ? strdup(name) : NULL;
}

static void dump_environ_to_log(const char *msg)
{
	char *buf = strvec_to_string(environ);

	if (buf) {
		LB_LOG(LB_LOGLEVEL_DEBUG, "%s: '%s'",
			msg, buf);
		free(buf);
	} else {
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"ENV: (failed to create string)");
	}
}

static void revert_to_user_version_of_env_var(
	const char *realvarname, const char *wrappedname)
{
	char *cp = getenv(wrappedname);

	if (cp) {
		LB_LOG(LB_LOGLEVEL_DEBUG, "Revert env.var:%s=%s (%s)",
			realvarname, cp, wrappedname);
		setenv(realvarname, cp, 1/*overwrite*/);
	} else {
		int r;
		LB_LOG(LB_LOGLEVEL_DEBUG, "Revert env.var:Clear %s", realvarname);
		r = unsetenv(realvarname);
		if(r < 0) {
			int e = errno;
			LB_LOG(LB_LOGLEVEL_ERROR, "unsetenv(%s) failed, errno=%d",
				realvarname, e);
		}
	}
	if (LB_LOG_IS_ACTIVE(LB_LOGLEVEL_NOISE3))
		dump_environ_to_log("revert_to_user_version_of_env_var: env. is now:");
}

/* lb_initialize_global_variables()
 *
 * NOTE: This function can be called before the environment
 * is available. This happens at least on Linux when the
 * pthreads library is initializing; it calls uname(), which
 * will be handled by our uname() wrapper (it has been prepared
 * for this situation, too)
 *
 * WARNING: Never call the logger from this function
 * before "lb_global_vars_initialized__" has been set!
 * (the result would be infinite recursion, because the logger wants
 * to open() the logfile..)
*/
void lb_initialize_global_variables(void)
{
	if (!lb_global_vars_initialized__) {
		char	*cp;

		if (!ldbox_session_dir) {
			cp = getenv("LDBOX_SESSION_DIR");
			if (cp) ldbox_session_dir = strdup(cp);
		}
		if (!ldbox_session_mode) {
			/* optional variable */
			cp = getenv("LDBOX_SESSION_MODE");
			if (cp) ldbox_session_mode = strdup(cp);
		}
		if (!ldbox_vperm_ids) {
			cp = getenv("LDBOX_VPERM_IDS");
			if (cp) ldbox_vperm_ids = strdup(cp);
		}
		if (!ldbox_network_mode) {
			/* optional variable */
			cp = getenv("LDBOX_NETWORK_MODE");
			if (cp) ldbox_network_mode = strdup(cp);
		}
		if (!ldbox_binary_name) {
			cp = getenv("__LB_BINARYNAME");
			if (cp) ldbox_binary_name = strdup(cp);
		}
		if (!ldbox_exec_name) {
			cp = getenv("__LB_EXEC_BINARYNAME");
			if (cp) ldbox_exec_name = strdup(cp);
		}
		if (!ldbox_real_binary_name) {
			cp = getenv("__LB_REAL_BINARYNAME");
			if (cp) ldbox_real_binary_name = strdup(cp);
		}
		if (!ldbox_orig_binary_name) {
			cp = getenv("__LB_ORIG_BINARYNAME");
			if (cp) ldbox_orig_binary_name = strdup(cp);
		}
		if (!ldbox_active_exec_policy_name) {
			cp = getenv("__LB_EXEC_POLICY_NAME");
			if (cp) ldbox_active_exec_policy_name = strdup(cp);
		}
		if (!ldbox_mapping_method) {
			cp = getenv("LDBOX_MAPPING_METHOD");
			if (cp) ldbox_mapping_method = strdup(cp);
		}
		if (!ldbox_chroot_path) {
			cp = getenv("__LB_CHROOT_PATH");
			if (cp) ldbox_chroot_path = strdup(cp);
		}

		if (ldbox_session_dir) {
			/* seems that we got it.. */
			lb_global_vars_initialized__ = 1;
			lblog_init();
			LB_LOG(LB_LOGLEVEL_DEBUG, "global vars initialized from env");

			/* check if the user wants us to SIGTRAP
			 * during liblb initialization.
			 *
			 * This is used together with two small GDB
			 * modification to make running gdb possible
			 * in ldbox emulation mode.
			 *
			 * The needed modifications in GDB:
			 *   - START_INFERIOR_TRAPS_EXPECTED=4 in gdb/inferior.h
			 *   - 'set environment LDBOX_SIGTRAP' at startup
			*/
			if (getenv("LDBOX_SIGTRAP"))
				raise(SIGTRAP);

			/* now when we know that the environment is
			 * valid, it is time to change LD_PRELOAD and
			 * LD_LIBRARY_PATH back to the values that the
			 * user expects to see.
			*/
			revert_to_user_version_of_env_var("LD_PRELOAD", "__LB_LD_PRELOAD");
			revert_to_user_version_of_env_var("LD_LIBRARY_PATH", "__LB_LD_LIBRARY_PATH");
			if(ldbox_chroot_path) {
				LB_LOG(LB_LOGLEVEL_DEBUG,
					"%s: Simulating chroot(%s)",
					__func__, ldbox_chroot_path);
			}
		}
	}
}

