-- Copyright (C) 2008 Lauri Leukkunen <lle@rahina.org>
-- Copyright (C) 2008 Nokia Corporation.
-- Licensed under MIT license.

-- "tools" mapping mode: Almost everything maps to tools_root.

-- Rule file interface version, mandatory.
--
rule_file_interface_version = "105"
----------------------------------

tools = tools_root
if (not tools) then
	tools = "/"
end

-- Don't map the working directory where ldbox was started, unless
-- that happens to be the root directory.
if ldbox_workdir == "/" then
	-- FIXME. There should be a way to skip a rule...
	unmapped_workdir = "/XXXXXX" 
else
	unmapped_workdir = ldbox_workdir
end

-- This mode can also be used to redirect /var/lib/dpkg/status to another
-- location (our dpkg-checkbuilddeps wrapper needs that)
var_lib_dpkg_status_actions = {
	{ if_env_var_is_not_empty = "LDBOX_TOOLS_MODE_VAR_LIB_DPKG_STATUS_LOCATION",
	  replace_by_value_of_env_var = "LDBOX_TOOLS_MODE_VAR_LIB_DPKG_STATUS_LOCATION",
	  protection = readonly_fs_if_not_root},

	-- Else use the default location
	{ map_to = tools_root, protection = readonly_fs_if_not_root}
}

-- These rules are used only if there is a separate tools_root
tools_in_subdir_mapping_rules = {
		{dir = session_dir, use_orig_path = true},

		{path = ldbox_cputransparency_cmd, use_orig_path = true,
		 readonly = true},

		{path = "/usr/bin/lb-show", use_orig_path = true,
		 readonly = true},
		{path = "/usr/bin/lb-ruletree", use_orig_path = true,
		 readonly = true},
		{dir = "/usr/lib/liblb", use_orig_path = true,
		 readonly = true},

		-- tools_root should not be mapped twice.
		{prefix = tools_root, use_orig_path = true, readonly = true},

		-- ldconfig is static binary, and needs to be wrapped
		{prefix = "/lb/wrappers",
		 replace_by = session_dir .. "/wrappers." .. active_mapmode,
		 readonly = true},

		--
		{prefix = "/var/run", map_to = session_dir},

		--
		{dir = "/tmp", map_to = session_dir},

		--
		{prefix = "/dev", use_orig_path = true},
		{dir = "/proc", custom_map_funct = lb_procfs_mapper,
		 virtual_path = true},
		{prefix = "/sys", use_orig_path = true},

		{prefix = ldbox_user_home_dir .. "/.ldbox",
		 use_orig_path = true},
		{prefix = ldbox_dir .. "/share/ldbox",
		 use_orig_path = true},

		--
		-- Following 3 rules are needed because package
		-- "resolvconf" makes resolv.conf to be symlink that
		-- points to /etc/resolvconf/run/resolv.conf and
		-- we want them all to come from host.
		--
		{prefix = "/var/run/resolvconf", force_orig_path = true,
		 readonly = true},
		{prefix = "/etc/resolvconf", force_orig_path = true,
		 readonly = true},
		{prefix = "/etc/resolv.conf", force_orig_path = true,
		 readonly = true},

		--
		{path = "/etc/passwd",
		 use_orig_path = true, readonly = true},

		-- -----------------------------------------------
		-- home directories = not mapped, R/W access
		{prefix = "/home", use_orig_path = true},

		-- -----------------------------------------------

		-- "policy-rc.d" checks if scratchbox-version exists, 
		-- to detect if it is running inside scratchbox..
		{prefix = "/scratchbox/etc/scratchbox-version",
		 replace_by = "/usr/share/ldbox/version",
		 log_level = "warning",
		 readonly = true, virtual_path = true},

		-- -----------------------------------------------

		{path = "/var/lib/dpkg/status", actions = var_lib_dpkg_status_actions},

		-- The default is to map everything to tools_root,
		-- except that we don't map the directory tree where
		-- ldbox was started.
		{prefix = unmapped_workdir, use_orig_path = true},

		{path = "/", use_orig_path = true},
		{prefix = "/", map_to = tools_root,
		 protection = readonly_fs_if_not_root}
}

if (tools_root == nil) or (tools_root == "") or (tools_root == "/") then
	-- Host tools used as tools root.
	-- Not really useful use case, but keeps this mode functional.
	fs_mapping_rules = {
		{prefix = "/", use_orig_path = true},
	}
else
	-- Tools in a subdir.
	fs_mapping_rules = tools_in_subdir_mapping_rules
end
