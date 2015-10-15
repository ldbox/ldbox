-- Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2012 Nokia Corporation.
-- Licensed under MIT license.

-- Rule file interface version, mandatory.
--
fs_rule_lib_interface_version = "105"
----------------------------------

-- /proc rules.

rule_lib_proc_rules = {
	-- We can't change times or attributes of host's /proc,
	-- but must pretend to be able to do so. Redirect the path
	-- to an existing, dummy location.
	{path = "/proc",
	 func_class = FUNC_CLASS_SET_TIMES,
	 set_path = session_dir.."/dummy_file", protection = readonly_fs_if_not_root },

	{path = "/proc/cpuinfo",
	 actions = {
	  {if_exists_then_replace_by = session_dir.."/cpuinfo",
	   protection = readonly_fs_always},
	  {use_orig_path = true,
	   protection = readonly_fs_always},
	 }
	},

	-- Default:
	{dir = "/proc", custom_map_funct = lb_procfs_mapper,
	 virtual_path = true},
}		 

return rule_lib_proc_rules

