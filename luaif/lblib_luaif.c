/*
 * Copyright (c) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 * Portion Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * Interfaces to lblib functions from Lua.
*/

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "mapping.h"
#include "lb.h"
#include "lb_network.h"
#include "liblb.h"
#include "exported.h"

#include "lblib_luaif.h"

/* "lb.log": interface from lua to the logging system.
 * Parameters:
 *  1. log level (string)
 *  2. log message (string)
*/
int lua_lb_log(lua_State *luastate)
{
	char	*logmsg;
	char	*loglevel;
	int	n = lua_gettop(luastate);

	if (n != 2) {
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"lb_log_from_lua: wrong number of params (%d)", n);
		lua_pushstring(luastate, NULL);
		return 1;
	}

	/* FIXME: is it necessary to use strdup here? */
	loglevel = strdup(lua_tostring(luastate, 1));
	logmsg = strdup(lua_tostring(luastate, 2));

	if(!strcmp(loglevel, "debug"))
		LB_LOG(LB_LOGLEVEL_DEBUG, ">> %s", logmsg);
	else if(!strcmp(loglevel, "info"))
		LB_LOG(LB_LOGLEVEL_INFO, "INFO: %s", logmsg);
	else if(!strcmp(loglevel, "warning"))
		LB_LOG(LB_LOGLEVEL_WARNING, "WARNING: %s", logmsg);
	else if(!strcmp(loglevel, "network"))
		LB_LOG(LB_LOGLEVEL_NETWORK, "NETWORK: %s", logmsg);
	else if(!strcmp(loglevel, "notice"))
		LB_LOG(LB_LOGLEVEL_NOTICE, "NOTICE: %s", logmsg);
	else if(!strcmp(loglevel, "error"))
		LB_LOG(LB_LOGLEVEL_ERROR, "ERROR: %s", logmsg);
	else if(!strcmp(loglevel, "noise"))
		LB_LOG(LB_LOGLEVEL_NOISE, ">>>>: %s", logmsg);
	else if(!strcmp(loglevel, "noise2"))
		LB_LOG(LB_LOGLEVEL_NOISE2, ">>>>>>: %s", logmsg);
	else if(!strcmp(loglevel, "noise3"))
		LB_LOG(LB_LOGLEVEL_NOISE3, ">>>>>>>>: %s", logmsg);
	else /* default to level "error"  */
		LB_LOG(LB_LOGLEVEL_ERROR, "%s", logmsg);

	free(loglevel);
	free(logmsg);

	lua_pushnumber(luastate, 1);
	return 1;
}

/* "lb.path_exists", to be called from lua code
 * returns true if file, directory or symlink exists at the specified real path,
 * false if not.
*/
int lua_lb_path_exists(lua_State *l)
{
	int n;

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushboolean(l, 0);
	} else {
		char	*path = strdup(lua_tostring(l, 1));
		int	result = lb_path_exists(path);

		lua_pushboolean(l, result);
		LB_LOG(LB_LOGLEVEL_DEBUG, "lua_lb_path_exists got %d",
			result);
		free(path);
	}
	return 1;
}

/* "lb.debug_messages_enabled", to be called from lua code
 * returns true if LB_LOG messages have been enabled for the debug levels
 * (debug,noise,noise2...)
*/
int lua_lb_debug_messages_enabled(lua_State *l)
{
	if (LB_LOG_IS_ACTIVE(LB_LOGLEVEL_DEBUG)) {
		lua_pushboolean(l, 1);
	} else {
		lua_pushboolean(l, 0);
	}
	return 1;
}

/* "lb.isprefix(a,b)", to be called from lua code
 * Return true if "a" is prefix of "b"
*/
int lua_lb_isprefix(lua_State *l)
{
	int	n = lua_gettop(l);

	if (n != 2) {
		lua_pushboolean(l, 0);
	} else {
		const char	*str_a = lua_tostring(l, 1);
		const char	*str_b = lua_tostring(l, 2);
		int	result = 0;

		if (str_a && str_b) {
			int	prefixlen = strlen(str_a);

			if (!strncmp(str_a, str_b, prefixlen)) result = 1;
		}

		LB_LOG(LB_LOGLEVEL_DEBUG, "lua_lb_isprefix '%s','%s' => %d",
			str_a, str_b, result);

		lua_pushboolean(l, result);
	}
	return 1;
}

/* "lb.test_path_match(path, rule.dir, rule.prefix, rule.path)":
 * This is used from find_rule(); implementing this in C improves preformance.
 * Returns min.path length if a match is found, otherwise returns -1
*/
int lua_lb_test_path_match(lua_State *l)
{
	int	n = lua_gettop(l);
	int	result = -1;

	if (n == 4) {
		const char	*str_path = lua_tostring(l, 1);
		const char	*str_rule_dir = lua_tostring(l, 2);
		const char	*str_rule_prefix = lua_tostring(l, 3);
		const char	*str_rule_path = lua_tostring(l, 4);
		const char	*match_type = "no match";

		if (str_path && str_rule_dir && (*str_rule_dir != '\0')) {
			int	prefixlen = strlen(str_rule_dir);

			/* test a directory prefix: the next char after the
			 * prefix must be '\0' or '/', unless we are accessing
			 * the root directory */
			if (!strncmp(str_path, str_rule_dir, prefixlen)) {
				if ( ((prefixlen == 1) && (*str_path=='/')) ||
				     (str_path[prefixlen] == '/') ||
				     (str_path[prefixlen] == '\0') ) {
					result = prefixlen;
					match_type = "dir";
				}
			}
		}
		if ((result == -1) && str_path
		    && str_rule_prefix && (*str_rule_prefix != '\0')) {
			int	prefixlen = strlen(str_rule_prefix);

			if (!strncmp(str_path, str_rule_prefix, prefixlen)) {
				result = prefixlen;
				match_type = "prefix";
			}
		}
		if ((result == -1) && str_path && str_rule_path) {
			int rule_path_len = strlen(str_rule_path);
			if (!strcmp(str_path, str_rule_path)) {
				result = rule_path_len;
				match_type = "path";
			} else {
				/* if "path" has a trailing slash, we may
				 * want to try again, ignoring the trailing
				 * slash. */
				int	path_len = strlen(str_path);
				if ((path_len > 2) &&
				    (str_path[path_len-1] == '/') &&
				    (path_len == (rule_path_len+1)) &&
				    !strncmp(str_path, str_rule_path, rule_path_len)) {
					result = rule_path_len;
					match_type = "path/";
				}
			}
		}
		LB_LOG(LB_LOGLEVEL_NOISE2,
			"lua_lb_test_path_match '%s','%s','%s' => %d (%s)",
			str_path, str_rule_prefix, str_rule_path,
			result, match_type);
	}
	lua_pushnumber(l, result);
	return 1;
}

/* "lb.readlink", to be called from lua code */
int lua_lb_readlink(lua_State *l)
{
	int n;
	char *path;
	char resolved_path[PATH_MAX + 1];

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	memset(resolved_path, '\0', PATH_MAX + 1);

	path = strdup(lua_tostring(l, 1));
	if (readlink(path, resolved_path, PATH_MAX) < 0) {
		free(path);
		lua_pushstring(l, NULL);
		return 1;
	} else {
		free(path);
		lua_pushstring(l, resolved_path);
		return 1;
	}
}

/* mappings from c to lua */
static const luaL_reg reg[] =
{
	{"log",				lua_lb_log},
	{"path_exists",			lua_lb_path_exists},
	{"debug_messages_enabled",	lua_lb_debug_messages_enabled},
	{"isprefix",			lua_lb_isprefix},
	{"test_path_match",		lua_lb_test_path_match},
	{"readlink",			lua_lb_readlink},
	{NULL,				NULL}
};

int lua_bind_lblib_functions(lua_State *l)
{
	luaL_register(l, "lblib", reg);
	lua_pushliteral(l,"version");
	lua_pushstring(l, "2.0" );
	lua_settable(l,-3);

	return 0;
}
