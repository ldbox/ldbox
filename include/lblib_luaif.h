/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Copyright (C) 2012 Nokia Corporation.
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef __LBLIB_LUAIF_H
#define __LBLIB_LUAIF_H

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

extern int lua_bind_lblib_functions(lua_State *l);
extern int lua_lb_log(lua_State *l);
extern int lua_lb_path_exists(lua_State *l);
extern int lua_lb_debug_messages_enabled(lua_State *l);
extern int lua_lb_isprefix(lua_State *l);
extern int lua_lb_test_path_match(lua_State *l);
extern int lua_lb_readlink(lua_State *l);

#endif
