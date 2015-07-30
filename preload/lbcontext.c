/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Copyright (C) 2012 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#include <string.h>
#include <libgen.h>
#define _GNU_SOURCE
#else
#include <string.h>
#include <libgen.h>
#endif

#include <limits.h>
#include <sys/param.h>
#include <sys/file.h>
#include <assert.h>

#include "mapping.h"
#include "lb.h"
#include "rule_tree.h"
#include "lb_network.h"
#include "lb_vperm.h"
#include "liblb.h"
#include "exported.h"


#define __set_errno(e) errno = e

static pthread_key_t lbcontext_key;
static pthread_once_t lbcontext_key_once = PTHREAD_ONCE_INIT;

static void alloc_lbcontext_key(void)
{
	if (pthread_key_create_fnptr)
#if 0
		(*pthread_key_create_fnptr)(&lbcontext_key, free_lua);
#else
		/* FIXME: Do we need a destructor? */
		(*pthread_key_create_fnptr)(&lbcontext_key, NULL);
#endif
}

/* used only if pthread lib is not available: */
static	struct lbcontext *my_lbcontext = NULL;

static struct lbcontext *alloc_lbcontext(void)
{
	struct lbcontext *tmp;

	if (pthread_getspecific_fnptr) {
		tmp = (*pthread_getspecific_fnptr)(lbcontext_key);
		if (tmp != NULL) {
			LB_LOG(LB_LOGLEVEL_DEBUG,
				"alloc_lbcontext: already done (pt-getspec.)");
			return(tmp);
		}
	} else if (my_lbcontext) {
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"alloc_lbcontext: already done (has my_lbcontext)");
		return(my_lbcontext);
	}

	tmp = malloc(sizeof(struct lbcontext));
	if (!tmp) {
		LB_LOG(LB_LOGLEVEL_ERROR,
			"alloc_lbcontext: Failed to allocate memory");
		return(NULL);
	}
	memset(tmp, 0, sizeof(struct lbcontext));

	if (pthread_setspecific_fnptr) {
		(*pthread_setspecific_fnptr)(lbcontext_key, tmp);
	} else {
		my_lbcontext = tmp;
	}
	
	if (!ldbox_session_dir || !*ldbox_session_dir) {
		LB_LOG(LB_LOGLEVEL_ERROR,
			"alloc_lbcontext: no LDBOX_SESSION_DIR");
		return(NULL); /* can't live without a session */
	}
	ldbox_session_dir = strdup(ldbox_session_dir);
	return(tmp);
}

static void increment_lbif_usage_counter(volatile struct lbcontext *ptr)
{
	if (LB_LOG_IS_ACTIVE(LB_LOGLEVEL_DEBUG)) {
		/* Well, to make this bullet-proof the lbif structure
		 * should be locked, but since this code is now used only for
		 * producing debugging information and the pointer is marked
		 * "volatile", the results are good enough. No need to slow 
		 * down anything with additional locks - this function is 
		 * called frequently. */
		if (ptr->lbcontext_in_use > 0) LB_LOG(LB_LOGLEVEL_DEBUG,
			"lbcontext already in use! (%d)",
			ptr->lbcontext_in_use);

		(ptr->lbcontext_in_use)++;
	}
}

void release_lbcontext(struct lbcontext *lbif)
{
	if (LB_LOG_IS_ACTIVE(LB_LOGLEVEL_DEBUG)) {
		int	i;
		volatile struct lbcontext *ptr = lbif;

		LB_LOG(LB_LOGLEVEL_NOISE, "release_lbcontext()");

		if (!ptr) {
			LB_LOG(LB_LOGLEVEL_DEBUG,
				"release_lbcontext(): ptr is NULL ");
			return;
		}

		i = ptr->lbcontext_in_use;
		if (i > 1) LB_LOG(LB_LOGLEVEL_DEBUG,
			"lbcontext usage counter was %d", i);

		(ptr->lbcontext_in_use)--;
	}
}

/* get access to ldbox context, create the structure
 * if it didn't exist; in that case, the structure is only
 * cleared.
 *
 * Remember to call release_lbcontext() after the
 * pointer is not needed anymore.
*/
struct lbcontext *get_lbcontext(void)
{
	struct lbcontext *ptr = NULL;

	if (!lb_global_vars_initialized__) lb_initialize_global_variables();

	if (!LB_LOG_INITIALIZED()) lblog_init();

	LB_LOG(LB_LOGLEVEL_NOISE, "get_lbcontext()");

	if (pthread_detection_done == 0) check_pthread_library();

	if (pthread_library_is_available) {
		if (pthread_once_fnptr)
			(*pthread_once_fnptr)(&lbcontext_key_once, alloc_lbcontext_key);
		if (pthread_getspecific_fnptr)
			ptr = (*pthread_getspecific_fnptr)(lbcontext_key);
		if (!ptr) ptr = alloc_lbcontext();
		if (!ptr) {
			LB_LOG(LB_LOGLEVEL_ERROR,
				"Something's wrong with"
				" the pthreads support");
			fprintf(stderr, "FATAL: lb preload library:"
				" Something's wrong with"
				" the pthreads support.\n");
			exit(1);
		}
	} else {
		/* no pthreads, single-thread application */
		ptr = my_lbcontext;
		if (!ptr) ptr = alloc_lbcontext();
		if (!ptr) {
			LB_LOG(LB_LOGLEVEL_ERROR,
				"Failed to get lbcontext"
				" (and the pthreads support is "
				" disabled!)");
			fprintf(stderr, "FATAL: lb preload library:"
				" Failed to get lbcontext"
				" (and the pthreads support is disabled!)\n");
			exit(1);
		}
	}

	if (LB_LOG_IS_ACTIVE(LB_LOGLEVEL_DEBUG)) {
		increment_lbif_usage_counter(ptr);
	}
	return(ptr);
}

/* Preload library constructor. Unfortunately this can
 * be called after other parts of this library have been called
 * if the program uses multiple threads (unbelievable, but true!),
 * so this isn't really too useful. Lua initialization was
 * moved to get_lbcontext_lua() because of this.
*/
#ifndef LB_TESTER
#ifdef __GNUC__
void lb_preload_library_constructor(void) __attribute((constructor));
#endif
#endif /* LB_TESTER */
void lb_preload_library_constructor(void)
{
	LB_LOG(LB_LOGLEVEL_DEBUG, "lb_preload_library_constructor called");
	lblog_init();
	LB_LOG(LB_LOGLEVEL_DEBUG, "lb_preload_library_constructor: done");
}

/* Return the library interface version string (used to be Lua/C if.vrs,
 * but it is still needed even if Lua is gone)
 * Note that this function is exported from liblb.so (for lb-show etc): */
const char *lb__lua_c_interface_version__(void)
{
	/* currently it is enough to return pointer to the constant string. */
	return(LB_LUA_C_INTERFACE_VERSION);
}

int test_if_str_in_colon_separated_list_from_env(
	const char *str, const char *env_var_name)
{
	int	result = 0;	/* boolean; default result is "not found" */
	char	*list;
	char	*tok = NULL;
	char	*tok_state = NULL;

	list = getenv(env_var_name);
	if (!list) {
		LB_LOG(LB_LOGLEVEL_DEBUG, "no %s", env_var_name);
		return(0);
	}
	list = strdup(list);	/* will be modified by strtok_r */
	LB_LOG(LB_LOGLEVEL_DEBUG, "%s is '%s'", env_var_name, list);

	tok = strtok_r(list, ":", &tok_state);
	while (tok) {
		result = !strcmp(str, tok);
		if (result) break; /* return if matched */
		tok = strtok_r(NULL, ":", &tok_state);
	}
	free(list);
	return(result);
}

