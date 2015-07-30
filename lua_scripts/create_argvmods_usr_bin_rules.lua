-- Copyright (c) 2009 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under MIT license

-- This script is executed after a new ldbox session has been created,
-- to create mapping rules for toolchain components:
-- For example, /usr/bin/gcc will be mapped to the toolchain. These rules
-- are used for other filesystem operations than exec*
-- (for example, when the shell is looking for a program, it must be
-- possible to stat() the destination)
--

gcc_rule_file_path = session_dir .. "/gcc-conf.lua"

default_rule = os.getenv("LDBOX_ARGVMODS_USR_BIN_DEFAULT_RULE")

function argvmods_to_mapping_rules(prefix)
	local n
	for n in pairs(argvmods) do
		local rule = argvmods[n]
		-- print("-- rule ", n, " new_filename=", rule.new_filename)
		local process_now = true
		if prefix ~= nil then
			if not lb.isprefix(prefix, n) then
				process_now = false
			end
		end

		if process_now and not rule.argvmods_processed then
			rule.argvmods_processed = true
			local k
			for k=1,table.maxn(rule.path_prefixes) do
				if rule.path_prefixes[k] == "/usr/bin/" and
				   rule.new_filename ~= nil then
					-- this rule maps "n" from /usr/bin to
					-- another file
					if lb.path_exists(rule.new_filename) then
						print("  {path=\"/usr/bin/"..n.."\",")
						print("   replace_by=\"" ..
							rule.new_filename.."\"},")
					else
						print("  -- WARNING: " ..
							rule.new_filename ..
							" does not exist")
					end
				end
			end
		end
	end
end

print("-- Argvmods-to-mapping-rules converter:")
print("-- Automatically generated mapping rules. Do not edit:")

-- Mode-specific fixed config settings to ruledb:
--
-- "enable_cross_gcc_toolchain" (default=true): All special processing
--     for the gcc-related tools (gcc,as,ld,..) will be disabled if set
--     to false.
--
ruletree.attach_ruletree()

local modename_in_ruletree = lb.get_forced_mapmode()
if modename_in_ruletree == nil then
	print("-- ERROR: modename_in_ruletree = nil")
	os.exit(14)
end

enable_cross_gcc_toolchain = true

tools = tools_root
if (not tools) then
	tools = "/"
end

if (tools == "/") then
        tools_prefix = ""
else
        tools_prefix = tools
end

do_file(session_dir .. "/share/ldbox/modes/"..modename_in_ruletree.."/config.lua")

ruletree.catalog_set("Conf."..modename_in_ruletree, "enable_cross_gcc_toolchain",
        ruletree.new_boolean(enable_cross_gcc_toolchain))

if exec_engine_loaded then
	print("-- Warning: exec engine was already loaded, will load again")
end
-- load the right argvmods_* file
do_file(session_dir .. "/lua_scripts/argvmods_loader.lua")
load_argvmods_file(nil)

-- Next, the argvmods stuff.

print("argvmods_rules_for_usr_bin_"..ldbox_cpu.." = {")
argvmods_to_mapping_rules(ldbox_cpu)
if (default_rule ~= nil) then
	print("  -- default:")
	print("  ", default_rule)
end
print("}")
local prefixrule1 = "  {prefix=\"/usr/bin/"..ldbox_cpu..
	"\",rules=argvmods_rules_for_usr_bin_"..ldbox_cpu.."},"
local prefixrule2 = ""

if ldbox_cpu ~= ldbox_uname_machine then
	print("argvmods_rules_for_usr_bin_"..ldbox_uname_machine.." = {")
	argvmods_to_mapping_rules(ldbox_uname_machine)
	if (default_rule ~= nil) then
		print("  -- default:")
		print("  ", default_rule)
	end
	print("}")
	prefixrule2 = "  {prefix=\"/usr/bin/"..ldbox_uname_machine..
		"\",rules=argvmods_rules_for_usr_bin_"..ldbox_uname_machine.."},"
end

print("argvmods_rules_for_usr_bin = {")
print(prefixrule1)
print(prefixrule2)
argvmods_to_mapping_rules(nil)
if (default_rule ~= nil) then
	print("  -- default:")
	print("  ", default_rule)
end
print("}")

print("-- End of rules created by argvmods-to-mapping-rules converter.")

