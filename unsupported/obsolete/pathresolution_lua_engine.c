/*
 * Copyright (c) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* pathresolution.c contains both versions (calls to C and Lua
 * mapping logic).
 * Either LB_PATHRESOLUTION_C_ENGINE or LB_PATHRESOLUTION_LUA_ENGINE
 * must be defined while compiling it.
*/
#define LB_PATHRESOLUTION_LUA_ENGINE
#include "pathresolution.c"

