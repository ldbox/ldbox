/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Portion Copyright (c) 2008 Nokia Corporation.
 * (exec postprocessing code implemented by Lauri T. Aarnio at Nokia)
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include <lb.h>
#include <mapping.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "processclock.h"

/* This stack dump routine is based on an example from the
 * book "Programming in Lua"
 *
 * - This uses logging level DEBUG, but the calls are usually
 *   enabled only at NOISE3.
*/
void dump_lua_stack(const char *msg, lua_State *L)
{
	int i;
	int top = lua_gettop(L);

	LB_LOG(LB_LOGLEVEL_DEBUG, "Stack dump/%s (gettop=%d):", msg, top);

	for (i = 1; i <= top; i++) {
		int t = lua_type(L, i);
		switch (t) {
		case LUA_TSTRING: /* strings */
			LB_LOG(LB_LOGLEVEL_DEBUG,
				"%d: '%s'", i, lua_tostring(L, i));
			break;

		case LUA_TBOOLEAN:  /* booleans */
			LB_LOG(LB_LOGLEVEL_DEBUG,
				"%d: %s", i,
				(lua_toboolean(L, i) ? "true" : "false"));
			break;

		case LUA_TNUMBER:  /* numbers */
			LB_LOG(LB_LOGLEVEL_DEBUG,
				"%d: %g", i, lua_tonumber(L, i));
			break;

		default:
			LB_LOG(LB_LOGLEVEL_DEBUG,
				"%d: %s", i, lua_typename(L, t));
			break;
		}
	}
}

#if 0
/* Convert a vector of strings to a lua table, leaves that table to
 * lua's stack.
*/
static void strvec_to_lua_table(struct lbcontext *lbctx, char **args)
{
	char	**p;
	int	i;

	lua_newtable(lbctx->lua);
	LB_LOG(LB_LOGLEVEL_NOISE2, "strvec_to_lua_table: ");
	for (p = args, i = 1; p && *p; p++, i++) {
		LB_LOG(LB_LOGLEVEL_NOISE2, "set element %d to '%s'", i, *p);
		lua_pushnumber(lbctx->lua, i);
		lua_pushstring(lbctx->lua, *p);
		lua_settable(lbctx->lua, -3);
	}
}
#endif

#if 0
void strvec_free(char **args)
{
	char **p;

	for (p = args; *p; p++) {
		free(*p);
	}
	free(args);
}
#endif

#if 0
/* convert a lua table (table of strings) to a string vector,
 * the vector will be dynamically allocated.
*/
void lua_string_table_to_strvec(lua_State *l,
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
#endif

#if 0
void lb_push_string_to_lua_stack(char *str)
{
	struct lbcontext *lbctx = get_lbcontext_lua();

	if (lbctx) {
		lua_pushstring(lbctx->lua, str);
		release_lbcontext(lbctx);
	}
}
#endif

#if 0
/* Exec Postprocessing:
*/
int lb_execve_postprocess(const char *exec_type,
	const char *exec_policy_name,
	char **mapped_file,
	char **filename,
	const char *binary_name,
	char ***argv,
	char ***envp)
{
	struct lbcontext *lbctx;
	int res, new_argc;
	int replace_environment = 0;
	PROCESSCLOCK(clk1)

	START_PROCESSCLOCK(LB_LOGLEVEL_INFO, &clk1, "lb_execve_postprocess");
	lbctx = get_lbcontext_lua();
	if (!lbctx) return(0);

	if(LB_LOG_IS_ACTIVE(LB_LOGLEVEL_NOISE3)) {
		dump_lua_stack("lb_execve_postprocess entry", lbctx->lua);
	}

	if (!argv || !envp) {
		LB_LOG(LB_LOGLEVEL_ERROR,
			"ERROR: lb_argvenvp: (argv || envp) == NULL");
		release_lbcontext(lbctx);
		return -1;
	}

	LB_LOG(LB_LOGLEVEL_NOISE,
		"lb_execve_postprocess: gettop=%d", lua_gettop(lbctx->lua));

	lua_getfield(lbctx->lua, LUA_GLOBALSINDEX, "lb_execve_postprocess");

	lua_pushstring(lbctx->lua, exec_policy_name);
	lua_pushstring(lbctx->lua, exec_type);
	lua_pushstring(lbctx->lua, *mapped_file);
	lua_pushstring(lbctx->lua, *filename);
	lua_pushstring(lbctx->lua, binary_name);
	strvec_to_lua_table(lbctx, *argv);
	strvec_to_lua_table(lbctx, *envp);

	/* args: exec_policy_name, exec_type, mapped_file, filename,
	 *	 binaryname, argv, envp
	 * returns: res, mapped_file, filename, argc, argv, envc, envp */
	lua_call(lbctx->lua, 7, 7);
	
	res = lua_tointeger(lbctx->lua, -7);
	switch (res) {

	case 0:
		/* exec arguments were modified, replace contents of
		 * argv vector */
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"lb_execve_postprocess: Updated argv&envp");
		free(*mapped_file);
		*mapped_file = strdup(lua_tostring(lbctx->lua, -6));

		free(*filename);
		*filename = strdup(lua_tostring(lbctx->lua, -5));

		strvec_free(*argv);
		new_argc = lua_tointeger(lbctx->lua, -4);
		lua_string_table_to_strvec(lbctx->lua, -3, argv, new_argc);

		replace_environment = 1;
		break;

	case 1:
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"lb_execve_postprocess: argv was not modified");
		/* always update environment when we are going to exec */
		replace_environment = 1;
		break;

	case -1:
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"lb_execve_postprocess: exec denied");
		break;

	default:
		LB_LOG(LB_LOGLEVEL_ERROR,
			"lb_execve_postprocess: Unsupported result %d", res);
		break;
	}

	if (replace_environment) {
		int new_envc;
		new_envc = lua_tointeger(lbctx->lua, -2);
		strvec_free(*envp);
		lua_string_table_to_strvec(lbctx->lua, -1, envp, new_envc);
	}

	/* remove lb_execve_postprocess return values from the stack.  */
	lua_pop(lbctx->lua, 7);

	STOP_AND_REPORT_PROCESSCLOCK(LB_LOGLEVEL_INFO, &clk1, mapped_file);

	LB_LOG(LB_LOGLEVEL_NOISE,
		"lb_execve_postprocess: at exit, gettop=%d", lua_gettop(lbctx->lua));
	release_lbcontext(lbctx);
	return res;
}
#endif

