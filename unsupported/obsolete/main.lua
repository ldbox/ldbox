-- ldbox Lua main file
-- Copyright (C) 2006, 2007 Lauri Leukkunen
-- Licensed under MIT license.

-- This file is loaded by the liblb.so preload library, from the
-- constructor to initialize lb's "Lua-side"

debug = os.getenv("LDBOX_MAPPING_DEBUG")
debug_messages_enabled = lb.debug_messages_enabled()
exec_engine_loaded = false
mapping_engine_loaded = false

-- This version string is used to check that the lua scripts offer 
-- what the C files expect, and v.v.
-- Increment the number whenever the interface beween Lua and C is changed.
--
-- NOTE: the corresponding identifier for C is in include/lb.h,
-- see that file for description about differences
lb_lua_c_interface_version = "127"

function do_file(filename)
	if (debug_messages_enabled) then
		lb.log("debug", string.format("Loading '%s'", filename))
	end
	f, err = loadfile(filename)
	if (f == nil) then
		error("\nError while loading " .. filename .. ": \n" 
			.. err .. "\n")
		-- "error()" never returns
	else
		f() -- execute the loaded chunk
	end
end

session_dir = lb.get_session_dir()

-- Load session-specific settings
do_file(session_dir .. "/lb-session.conf")

-- Set global variables: Config file paths, etc.

local forced_modename = lb.get_forced_mapmode()

-- rule_file_path and rev_rule_file_path are global varibales
if forced_modename == nil or forced_modename == "Default" then
	-- set active_mapmode to the real name of the mapping mode,
	-- even if forced_modename happens to be "Default"
	active_mapmode = ldbox_mapmode
else
	active_mapmode = forced_modename
end

rule_file_path = session_dir .. "/rules/" .. active_mapmode .. ".lua"
rev_rule_file_path = session_dir .. "/rev_rules/" .. active_mapmode .. ".lua"
exec_rule_file_path = session_dir .. "/exec_rules/" .. active_mapmode .. ".lua"

target_root = ldbox_target_root
if (not target_root or target_root == "") then
	target_root = "/"
end

tools_root = ldbox_tools_root
if (tools_root == "") then
	tools_root = nil
end

-- other processes than "make" or the shells load
-- argvenvp.lua only if exec* functions are needed!
--
-- Also, "mapping.lua" is loaded only when needed.

--function lb_execve_postprocess_loader(rule, exec_policy, exec_type,
--		mapped_file, filename, binaryname, argv, envp)
--	local prev_fn = lb_execve_postprocess
--
--	lb.log("info", "lb_execve_postprocess called: loading argvenvp.lua")
--	do_file(session_dir .. "/lua_scripts/argvenvp.lua")
--
--	if prev_fn == lb_execve_postprocess then
--		lb.log("error",
--			"Fatal: Failed to load real lb_execve_postprocess")
--		os.exit(88)
--	end
--
--	-- This loader has been replaced. The following call is not
--	-- a recursive call to this function, even if it may look like one:
--	return lb_execve_postprocess(rule, exec_policy, exec_type,
--		mapped_file, filename, binaryname, argv, envp)
--end

function ldbox_get_mapping_requirements(binary_name, func_name, full_path, fn_class)
	local prev_fn = ldbox_get_mapping_requirements

	lb.log("info", "ldbox_get_mapping_requirements called: loading mapping.lua")
	do_file(session_dir .. "/lua_scripts/mapping.lua")

	if prev_fn == ldbox_get_mapping_requirements then
		lb.log("error",
			"Fatal: Failed to load real ldbox_get_mapping_requirements")
		os.exit(88)
	end

	-- This loader has been replaced. The following call is not
	-- a recursive call to this function, even if it may look like one:
	return ldbox_get_mapping_requirements(binary_name, func_name, full_path, fn_class)
end

function ldbox_reverse_path(binary_name, func_name, full_path, fn_class)
	local prev_fn = ldbox_reverse_path

	lb.log("info", "ldbox_reverse_path called: loading mapping.lua")
	do_file(session_dir .. "/lua_scripts/mapping.lua")

	if prev_fn == ldbox_reverse_path then
		lb.log("error",
			"Fatal: Failed to load real ldbox_reverse_path")
		os.exit(88)
	end

	-- This loader has been replaced. The following call is not
	-- a recursive call to this function, even if it may look like one:
	return ldbox_reverse_path(binary_name, func_name, full_path, fn_class)
end

local binary_name = lb.get_binary_name()

-- This proformance hack is now disabled:
-- although all exec-related code is not yet in C,
-- native binaries don't need Lua code anymore.
--
--if (binary_name == "make") or
--   (binary_name == "sh") or
--   (binary_name == "bash") or
--   (binary_name == "gmake") or
--   (binary_name == "xargs") or
--   (binary_name == "gcc") or
--   (binary_name == "find") then
--	-- This is a performance optimization;
--	-- this process will probably do multiple fork()+exec*() calls,
--	-- so it is better to load the exec code right now.
--	-- otherwise every child process will be doing the loading..that
--	-- would work, of course, but this is better when the overall
--	-- performance is considered.
--	if debug_messages_enabled then
--		lb.log("debug", "Loading exec code now")
--	end
--	do_file(session_dir .. "/lua_scripts/argvenvp.lua")
--else
	--if debug_messages_enabled then
	--	lb.log("debug", "exec code will be loaded on demand")
	--end
	--lb_execve_postprocess = lb_execve_postprocess_loader
--end

-- lb is ready for operation!
