/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Portion Copyright (c) 2008 Nokia Corporation.
 * (symlink- and path resolution code refactored by Lauri T. Aarnio at Nokia)
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * Path mapping subsystem of ldbox; interfaces to Lua functions.
*/

#if 0
#include <unistd.h>
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
#include <pthread.h>
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <mapping.h>
#include <lb.h>
#include "liblb.h"
#include "exported.h"

#if 0
#ifdef EXTREME_DEBUGGING
#include <execinfo.h>
#endif
#endif

#include "pathmapping.h" /* get private definitions of this subsystem */

static void check_mapping_flags(int flags, const char *fn)
{
	if (flags & (~LB_MAPPING_RULE_ALL_FLAGS)) {
		LB_LOG(LB_LOGLEVEL_WARNING,
			"%s returned unknown flags (0%o)",
			fn, flags & (~LB_MAPPING_RULE_ALL_FLAGS));
	}
}

/* ========== Interfaces to Lua functions: ========== */

/* note: this expects that the lua stack already contains the mapping rule,
 * needed by ldbox_translate_path (lua code).
 * at exit the rule is still there.
*/
char *call_lua_function_ldbox_translate_path(
	const path_mapping_context_t *ctx,
	int result_log_level,
	const char *abs_clean_virtual_path,
	int *flagsp,
	char **exec_policy_name_ptr)
{
	struct lbcontext	*lbctx = ctx->pmc_lbctx;
	int flags;
	char *host_path = NULL;

	LB_LOG(LB_LOGLEVEL_NOISE, "calling ldbox_translate_path for %s(%s), fn_class=0x%X",
		ctx->pmc_func_name, abs_clean_virtual_path, ctx->pmc_fn_class);
	if (!lbctx->lua) lbcontext_initialize_lua(lbctx);
	LB_LOG(LB_LOGLEVEL_NOISE,
		"call_lua_function_ldbox_translate_path: gettop=%d",
		lua_gettop(lbctx->lua));
	if(LB_LOG_IS_ACTIVE(LB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("call_lua_function_ldbox_translate_path entry",
			lbctx->lua);
	}

	lua_getfield(lbctx->lua, LUA_GLOBALSINDEX, "ldbox_translate_path");
	/* stack now contains the rule object and string "ldbox_translate_path",
         * move the string to the bottom: */
	lua_insert(lbctx->lua, -2);
	/* add other parameters */
	lua_pushstring(lbctx->lua, ctx->pmc_binary_name);
	lua_pushstring(lbctx->lua, ctx->pmc_func_name);
	lua_pushstring(lbctx->lua, abs_clean_virtual_path);
	lua_pushnumber(lbctx->lua, ctx->pmc_fn_class);
	 /* 5 arguments, returns rule,policy,path,flags */
	lua_call(lbctx->lua, 5, 4);

	host_path = (char *)lua_tostring(lbctx->lua, -2);
	if (host_path && (*host_path != '/')) {
		LB_LOG(LB_LOGLEVEL_ERROR,
			"Mapping failed: Result is not absolute ('%s'->'%s')",
			abs_clean_virtual_path, host_path);
		host_path = NULL;
	} else if (host_path) {
		host_path = strdup(host_path);
	}
	flags = lua_tointeger(lbctx->lua, -1);
	check_mapping_flags(flags, "ldbox_translate_path");
	if (flagsp) *flagsp = flags;

	if (exec_policy_name_ptr) {
		char *exec_policy_name;

		if (*exec_policy_name_ptr) {
			free(*exec_policy_name_ptr);
			*exec_policy_name_ptr = NULL;
		}
		exec_policy_name = (char *)lua_tostring(lbctx->lua, -3);
		if (exec_policy_name) {
			*exec_policy_name_ptr = strdup(exec_policy_name);
		}
	}

	lua_pop(lbctx->lua, 3); /* leave the rule to the stack */

	if (host_path) {
		char *new_host_path = clean_and_log_fs_mapping_result(ctx,
			abs_clean_virtual_path, result_log_level, host_path, flags);
		free(host_path);
		host_path = new_host_path;
	}
	if (!host_path) {
		LB_LOG(LB_LOGLEVEL_ERROR,
			"No result from ldbox_translate_path for: %s '%s'",
			ctx->pmc_func_name, abs_clean_virtual_path);
	}
	LB_LOG(LB_LOGLEVEL_NOISE,
		"call_lua_function_ldbox_translate_path: at exit, gettop=%d",
		lua_gettop(lbctx->lua));
	if(LB_LOG_IS_ACTIVE(LB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("call_lua_function_ldbox_translate_path exit",
			lbctx->lua);
	}
	return(host_path);
}

/* - returns 1 if ok (then *min_path_lenp is valid)
 * - returns 0 if failed to find the rule
 * Note: this leaves the rule to the stack!
*/
int call_lua_function_ldbox_get_mapping_requirements(
	const path_mapping_context_t *ctx,
	const struct path_entry_list *abs_virtual_source_path_list,
	int *min_path_lenp,
	int *call_translate_for_all_p)
{
	struct lbcontext	*lbctx = ctx->pmc_lbctx;
	int rule_found;
	int min_path_len;
	int flags;
	char	*abs_virtual_source_path_string;

	abs_virtual_source_path_string = path_list_to_string(abs_virtual_source_path_list);

	LB_LOG(LB_LOGLEVEL_NOISE,
		"calling ldbox_get_mapping_requirements for %s(%s)",
		ctx->pmc_func_name, abs_virtual_source_path_string);
	if (!lbctx->lua) lbcontext_initialize_lua(lbctx);
	LB_LOG(LB_LOGLEVEL_NOISE,
		"call_lua_function_ldbox_get_mapping_requirements: gettop=%d",
		lua_gettop(lbctx->lua));

	lua_getfield(lbctx->lua, LUA_GLOBALSINDEX,
		"ldbox_get_mapping_requirements");
	lua_pushstring(lbctx->lua, ctx->pmc_binary_name);
	lua_pushstring(lbctx->lua, ctx->pmc_func_name);
	lua_pushstring(lbctx->lua, abs_virtual_source_path_string);
	lua_pushnumber(lbctx->lua, ctx->pmc_fn_class);
	/* 4 arguments, returns 4: (rule, rule_found_flag,
	 * min_path_len, flags) */
	lua_call(lbctx->lua, 4, 4);

	rule_found = lua_toboolean(lbctx->lua, -3);
	min_path_len = lua_tointeger(lbctx->lua, -2);
	flags = lua_tointeger(lbctx->lua, -1);
	check_mapping_flags(flags, "ldbox_get_mapping_requirements");
	if (min_path_lenp) *min_path_lenp = min_path_len;
	if (call_translate_for_all_p)
		*call_translate_for_all_p =
			(flags & LB_MAPPING_RULE_FLAGS_CALL_TRANSLATE_FOR_ALL);

	/* remove last 3 values; leave "rule" to the stack */
	lua_pop(lbctx->lua, 3);

	LB_LOG(LB_LOGLEVEL_DEBUG, "ldbox_get_mapping_requirements -> %d,%d,0%o",
		rule_found, min_path_len, flags);

	LB_LOG(LB_LOGLEVEL_NOISE,
		"call_lua_function_ldbox_get_mapping_requirements:"
		" at exit, gettop=%d",
		lua_gettop(lbctx->lua));
	free(abs_virtual_source_path_string);
	return(rule_found);
}

/* returns virtual_path */
char *call_lua_function_ldbox_reverse_path(
	const path_mapping_context_t *ctx,
	const char *abs_host_path)
{
	struct lbcontext	*lbctx = ctx->pmc_lbctx;
	char *virtual_path = NULL;
	int flags;

	LB_LOG(LB_LOGLEVEL_NOISE, "calling ldbox_reverse_path for %s(%s)",
		ctx->pmc_func_name, abs_host_path);
	if (!lbctx->lua) lbcontext_initialize_lua(lbctx);

	lua_getfield(lbctx->lua, LUA_GLOBALSINDEX, "ldbox_reverse_path");
	lua_pushstring(lbctx->lua, ctx->pmc_binary_name);
	lua_pushstring(lbctx->lua, ctx->pmc_func_name);
	lua_pushstring(lbctx->lua, abs_host_path);
	lua_pushnumber(lbctx->lua, ctx->pmc_fn_class);
	 /* 4 arguments, returns virtual_path and flags */
	lua_call(lbctx->lua, 4, 2);

	virtual_path = (char *)lua_tostring(lbctx->lua, -2);
	if (virtual_path) {
		virtual_path = strdup(virtual_path);
	}

	flags = lua_tointeger(lbctx->lua, -1);
	check_mapping_flags(flags, "ldbox_reverse_path");
	/* Note: "flags" is not yet used for anything, intentionally */
 
	lua_pop(lbctx->lua, 2); /* remove return values */

	if (virtual_path) {
		LB_LOG(LB_LOGLEVEL_DEBUG, "virtual_path='%s'", virtual_path);
	} else {
		LB_LOG(LB_LOGLEVEL_INFO,
			"No result from ldbox_reverse_path for: %s '%s'",
			ctx->pmc_func_name, abs_host_path);
	}
	return(virtual_path);
}

/* clean up path resolution environment from lua stack */
void drop_rule_from_lua_stack(struct lbcontext *lbctx)
{
	if (!lbctx->lua) lbcontext_initialize_lua(lbctx);
	lua_pop(lbctx->lua, 1);

	LB_LOG(LB_LOGLEVEL_NOISE,
		"drop rule from stack: at exit, gettop=%d",
		lua_gettop(lbctx->lua));
}

