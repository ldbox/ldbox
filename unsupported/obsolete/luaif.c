/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "mapping.h"
#include "lb.h"
#include "rule_tree.h"
#include "rule_tree_lua.h"
#include "lb_network.h"
#include "lb_vperm.h"
#include "liblb.h"
#include "exported.h"


#define __set_errno(e) errno = e

void mapping_log_write(char *msg);
static int lua_bind_lb_functions(lua_State *l);

static pthread_key_t lbcontext_key;
static pthread_once_t lbcontext_key_once = PTHREAD_ONCE_INIT;

static char *read_string_variable_from_lua(
	struct lbcontext *lbif,
	const char *name)
{
	char *result = NULL;

	if (lbif && name && *name) {
		lua_getglobal(lbif->lua, name);
		result = (char *)lua_tostring(lbif->lua, -1);
		if (result) {
			result = strdup(result);
		}
		lua_pop(lbif->lua, 1);
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"Lua variable %s = '%s', gettop=%d",
			name, (result ? result : "<NULL>"),
			lua_gettop(lbif->lua));
	}
	return(result);
}

static void free_lua(void *buf)
{
	free(buf);
}

static void alloc_lbcontext_key(void)
{
	if (pthread_key_create_fnptr)
		(*pthread_key_create_fnptr)(&lbcontext_key, free_lua);
}

/* used only if pthread lib is not available: */
static	struct lbcontext *my_lbcontext = NULL;

static void load_and_execute_lua_file(struct lbcontext *lbif, const char *filename)
{
	const char *errmsg;

	switch(luaL_loadfile(lbif->lua, filename)) {
	case LUA_ERRFILE:
		fprintf(stderr, "Error loading %s\n", filename);
		exit(1);
	case LUA_ERRSYNTAX:
		errmsg = lua_tostring(lbif->lua, -1);
		fprintf(stderr, "Syntax error in %s (%s)\n", filename, 
			(errmsg?errmsg:""));
		exit(1);
	case LUA_ERRMEM:
		fprintf(stderr, "Memory allocation error while "
				"loading %s\n", filename);
		exit(1);
	default:
		;
	}
	lua_call(lbif->lua, 0, 0);
}

/* Lua calls this at panic: */
static int lb_lua_panic(lua_State *l)
{
	fprintf(stderr,
		"ldbox: Lua interpreter PANIC: unprotected error in call to Lua API (%s)\n",
		lua_tostring(l, -1));
	lblog_init(); /* make sure the logger has been initialized */
	LB_LOG(LB_LOGLEVEL_ERROR,
		"Lua interpreter PANIC: unprotected error in call to Lua API (%s)\n",
		lua_tostring(l, -1));
	return 0;
}

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
	tmp->lua = NULL;
	return(tmp);
}

void lbcontext_initialize_lua(struct lbcontext *lbctx)
{
	char *main_lua_script = NULL;
	char *lua_if_version = NULL;

	/* return immediately if already been here */
	if (!lbctx || lbctx->lua) return;

	if (asprintf(&main_lua_script, "%s/lua_scripts/main.lua",
	     ldbox_session_dir) < 0) {
		LB_LOG(LB_LOGLEVEL_ERROR,
			"lbcontext_initialize_lua: asprintf failed to allocate memory");
		return;
	}
		
	LB_LOG(LB_LOGLEVEL_INFO, "Loading '%s'", main_lua_script);

	lbctx->lua = luaL_newstate();
	lua_atpanic(lbctx->lua, lb_lua_panic);

	disable_mapping(lbctx);
	luaL_openlibs(lbctx->lua);
	lua_bind_lb_functions(lbctx->lua); /* register our lb_ functions */
	lua_bind_ruletree_functions(lbctx->lua); /* register our ruletree_ functions */
	lua_bind_lblib_functions(lbctx->lua); /* register lblib.* functions */

	load_and_execute_lua_file(lbctx, main_lua_script);

	enable_mapping(lbctx);

	/* check Lua/C interface version. */
	lua_if_version = read_string_variable_from_lua(lbctx,
		"lb_lua_c_interface_version");
	if (!lua_if_version) {
		LB_LOG(LB_LOGLEVEL_ERROR, "FATAL ERROR: "
			"lb's Lua scripts didn't provide"
			" 'lb_lua_c_interface_version' identifier!");
		exit(1);
	}
	if (strcmp(lua_if_version, LB_LUA_C_INTERFACE_VERSION)) {
		LB_LOG(LB_LOGLEVEL_ERROR, "FATAL ERROR: "
			"lb's Lua script interface version mismatch:"
			" scripts provide '%s', but '%s' was expected",
			lua_if_version, LB_LUA_C_INTERFACE_VERSION);
		exit(1);
	}
	free(lua_if_version);

	LB_LOG(LB_LOGLEVEL_INFO, "lua initialized.");
	LB_LOG(LB_LOGLEVEL_NOISE, "gettop=%d", lua_gettop(lbctx->lua));

	free(main_lua_script);
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
			"Lua instance already in use! (%d)",
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
			"Lua instance usage counter was %d", i);

		(ptr->lbcontext_in_use)--;
	}
}

/* get access to ldbox context, create the structure
 * if it didn't exist; in that case, the structure is only
 * cleared (most notably, the Lua system is not initialized
 * by this routine!)
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
				"Failed to get Lua instance"
				" (and the pthreads support is "
				" disabled!)");
			fprintf(stderr, "FATAL: lb preload library:"
				" Failed to get Lua instance"
				" (and the pthreads support is disabled!)\n");
			exit(1);
		}
	}

	if (LB_LOG_IS_ACTIVE(LB_LOGLEVEL_DEBUG)) {
		increment_lbif_usage_counter(ptr);
	}
	return(ptr);
}

/* get access to ldbox context, and make sure that Lua
 * has been initialized.
*/
struct lbcontext *get_lbcontext_lua(void)
{
	struct lbcontext *ptr = get_lbcontext();

	if (ptr) lbcontext_initialize_lua(ptr);
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

/* Read string variables from lua.
 * Note that this function is exported from liblb.so (for lb-show etc): */
char *lb__read_string_variable_from_lua__(const char *name)
{
	struct lbcontext *lbif;
	char *cp;

	lbif = get_lbcontext_lua();
	cp = read_string_variable_from_lua(lbif, name);
	release_lbcontext(lbif);
	return(cp);
}

/* Return the Lua/C interface version string = the library interface version.
 * Note that this function is exported from liblb.so (for lb-show etc): */
const char *lb__lua_c_interface_version__(void)
{
	/* currently it is enough to return pointer to the constant string. */
	return(LB_LUA_C_INTERFACE_VERSION);
}

/* Read and execute an lua file. Used from lb-show, useful
 * for debugging and benchmarking since the script is executed
 * in a context which already contains all ldbox's varariables etc.
 * Note that this function is exported from liblb.so:
*/
void lb__load_and_execute_lua_file__(const char *filename)
{
	struct lbcontext *lbif;

	lbif = get_lbcontext_lua();
	load_and_execute_lua_file(lbif, filename);
	release_lbcontext(lbif);
}

#if 0 /* DISABLED 2008-10-23/LTA: lb_decolonize_path() is not currently available*/
/* "lb.decolonize_path", to be called from lua code */
static int lua_lb_decolonize_path(lua_State *l)
{
	int n;
	char *path;
	char *resolved_path;

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	resolved_path = lb_decolonize_path(path);
	lua_pushstring(l, resolved_path);
	free(resolved_path);
	free(path);
	return 1;
}
#endif

static void lua_string_table_to_strvec(lua_State *l,
	int lua_stack_offs, char ***args, int new_argc)
{
	int	i;

	*args = calloc(new_argc + 1, sizeof(char *));

	for (i = 0; i < new_argc; i++) {
		lua_rawgeti(l, lua_stack_offs, i + 1);
		(*args)[i] = strdup(lua_tostring(l, -1));
		lua_pop(l, 1); /* return stack state to what it
					 * was before lua_rawgeti() */
	}
	(*args)[i] = NULL;
}

static void strvec_free(char **args)
{
	char **p;

	for (p = args; *p; p++) {
		free(*p);
	}
	free(args);
}

/* Make preparations for an union directory:
 * (FIXME. This is not very efficient)
 * (FIXME. Does not remove removed entries)
 * (FIXME. add description)
 *
 * Parameters:
 * 1. Path to the directory, which is presented as
 *    union of more than one real directories
 * 2. Number of real directory paths
 * 3. Array of real directory paths
 * Returns:
 * 1. Status (boolean): false if error, true if ok
 * 2. Path to the created union directory.
*/
static int lua_lb_prep_union_dir(lua_State *l)
{
	char *dst_path = NULL;
	int num_real_dir_entries;
	char **src_paths = NULL;
	char *result_path = NULL;

	int n = lua_gettop(l);
	
	if (n == 3) {
		dst_path = strdup(lua_tostring(l, 1));
		num_real_dir_entries = lua_tointeger(l, 2);

		if (dst_path && (num_real_dir_entries > 0)) {
			lua_string_table_to_strvec(l, 3, &src_paths, num_real_dir_entries);
			result_path = prep_union_dir(dst_path, (const char **)src_paths, num_real_dir_entries);
			strvec_free(src_paths);
		}
	}
	lua_pushboolean(l, result_path ? 1 : 0);
	lua_pushstring(l, result_path);
	free(dst_path);
	if (result_path) free(result_path);
	return 2;
}

#if 0 /* Not used anymore. */
/* "lb.getdirlisting", to be called from lua code */
static int lua_lb_getdirlisting(lua_State *l)
{
	DIR *d;
	struct dirent *de;
	char *path;
	int count;

	int n = lua_gettop(l);
	
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	if ( (d = opendir(path)) == NULL ) {
		lua_pushstring(l, NULL);
		return 1;
	}
	count = 1; /* Lua indexes tables from 1 */
	lua_newtable(l); /* create a new table on the stack */
	
	while ( (de = readdir(d)) != NULL) { /* get one dirent at a time */
		lua_pushnumber(l, count);
		lua_pushstring(l, de->d_name); /* push the entries to
						* lua stack */
		lua_settable(l, -3);
		count++;
	}
	closedir(d);

	free(path);
	return 1;
}
#endif

static int lua_lb_setenv(lua_State *luastate)
{
	int	n = lua_gettop(luastate);

	if (n != 2) {
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"lb_log_from_lua: wrong number of params (%d)", n);
		lua_pushstring(luastate, NULL);
		return 1;
	}
	setenv(strdup(lua_tostring(luastate, 1)), strdup(lua_tostring(luastate, 2)), 1);
	return 1;
}

/* "lb.getcwd", to be called from lua code */
static int lua_lb_getcwd(lua_State *l)
{
	char cwd[PATH_MAX + 1];

	if (getcwd_nomap_nolog(cwd, sizeof(cwd))) {
		lua_pushstring(l, cwd);
	} else {
		lua_pushstring(l, NULL);
	}
	return 1;
}

/* "lb.get_binary_name", to be called from lua code */
static int lua_lb_get_binary_name(lua_State *l)
{
	lua_pushstring(l, ldbox_binary_name);
	return 1;
}

/* "lb.get_active_exec_policy_name", to be called from lua code */
static int lua_lb_get_active_exec_policy_name(lua_State *l)
{
	lua_pushstring(l, ldbox_active_exec_policy_name);
	return 1;
}

/* "lb.get_forced_mapmode", to be called from lua code */
static int lua_lb_get_forced_mapmode(lua_State *l)
{
	if (ldbox_session_mode) lua_pushstring(l, ldbox_session_mode);
	else lua_pushnil(l);
	return 1;
}

/* "lb.get_forced_network_mode", to be called from lua code */
static int lua_lb_get_forced_network_mode(lua_State *l)
{
	if (ldbox_network_mode) lua_pushstring(l, ldbox_network_mode);
	else lua_pushnil(l);
	return 1;
}

/* "lb.get_session_perm", to be called from lua code */
/* DEPRECATED. */
static int lua_lb_get_session_perm(lua_State *l)
{
	if (vperm_geteuid() == 0) lua_pushstring(l, "root");
	else lua_pushnil(l);
	return 1;
}

/* "lb.get_session_dir", to be called from lua code */
static int lua_lb_get_session_dir(lua_State *l)
{
	if (ldbox_session_dir) lua_pushstring(l, ldbox_session_dir);
	else lua_pushnil(l);
	return 1;
}

/* lb.test_fn_class_match(fn_class, rule.func_class)
 * - returns true if fn_class is in rule.func_class
 * (Lua does not have bitwise operators)
*/
static int lua_lb_test_fn_class_match(lua_State *l)
{
	int	n = lua_gettop(l);
	int	result = -1;

	if (n == 2) {
		int fn_class = lua_tointeger(l, 1);
		int rule_fn_class = lua_tointeger(l, 2);

		result = ((fn_class & rule_fn_class) != 0);
		LB_LOG(LB_LOGLEVEL_NOISE,
			"test_fn_class_match 0x%X,0x%X => %d",
			fn_class, rule_fn_class, result);
	}
	lua_pushnumber(l, result);
	return 1;
}

/* "lb.procfs_mapping_request", to be called from lua code */
static int lua_lb_procfs_mapping_request(lua_State *l)
{
	int n;
	char *path;
	char *resolved_path;

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	resolved_path = procfs_mapping_request(path);

	if (resolved_path) {
		/* mapped to somewhere else */
		lua_pushstring(l, resolved_path);
		free(resolved_path);
	} else {
		/* no need to map this path */
		lua_pushnil(l);
	}
	free(path);
	return 1;
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

/* "lb.test_if_listed_in_envvar", to be called from lua code
 * Parameters (in stack):
 *	1. string: unmapped path
 *	2. string: name of environment variable containing colon-separated list
 * Returns (in stack):
 *	1. flag (boolean): true if the path is listed in the environment
 *			   variable "LDBOX_REDIRECT_IGNORE", false otherwise
 * (this is used to examine values of "LDBOX_REDIRECT_IGNORE" and
 * "LDBOX_REDIRECT_FORCE")
 *
 * Note: these variables typically can't be cached
 * they can be changed by the current process.
*/
static int lua_lb_test_if_listed_in_envvar(lua_State *l)
{
	int result = 0; /* boolean; default result is "false" */
	int n;
	const char *path = NULL;
	const char *env_var_name = NULL;

	n = lua_gettop(l);
	if (n != 2) {
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"test_if_listed_in_envvar FAILS: lua_gettop = %d", n);
	} else {
		path = lua_tostring(l, 1);
		env_var_name = lua_tostring(l, 2);
		if (path && env_var_name) {
			result = test_if_str_in_colon_separated_list_from_env(
				path, env_var_name);
		}
	}
	lua_pushboolean(l, result);
	LB_LOG(LB_LOGLEVEL_DEBUG, "test_if_listed_in_envvar(%s) => %d",
		path, result);
	return 1;
}

/* mappings from c to lua */
static const luaL_reg reg[] =
{
	{"prep_union_dir",		lua_lb_prep_union_dir},
#if 0
	{"getdirlisting",		lua_lb_getdirlisting},
#endif
	{"readlink",			lua_lb_readlink},
#if 0
	{"decolonize_path",		lua_lb_decolonize_path},
#endif
	{"log",				lua_lb_log},
	{"setenv",			lua_lb_setenv},
	{"path_exists",			lua_lb_path_exists},
	{"debug_messages_enabled",	lua_lb_debug_messages_enabled},
	{"getcwd",			lua_lb_getcwd},
	{"get_binary_name",		lua_lb_get_binary_name},
	{"get_active_exec_policy_name",	lua_lb_get_active_exec_policy_name},
	{"get_forced_mapmode",		lua_lb_get_forced_mapmode},
	{"get_forced_network_mode",	lua_lb_get_forced_network_mode},
	{"get_session_perm",		lua_lb_get_session_perm}, /* DEPRECATED. */
	{"get_session_dir",		lua_lb_get_session_dir},
	{"isprefix",			lua_lb_isprefix},
	{"test_path_match",		lua_lb_test_path_match},
	{"test_fn_class_match",		lua_lb_test_fn_class_match},
	{"procfs_mapping_request",	lua_lb_procfs_mapping_request},
	{"test_if_listed_in_envvar",	lua_lb_test_if_listed_in_envvar},
	{NULL,				NULL}
};


static int lua_bind_lb_functions(lua_State *l)
{
	luaL_register(l, "lb", reg);
	lua_pushliteral(l,"version");
	lua_pushstring(l, "2.0" );
	lua_settable(l,-3);

	return 0;
}
