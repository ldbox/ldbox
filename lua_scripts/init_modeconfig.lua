-- Copyright (c) 2009,2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- Licensed under LGPL version 2.1, see top level LICENSE file for details.
--
-- Read in {all_needed_modes}/config.lua files and insert
-- rules and settings to the rule tree db.

for m_index,m_name in pairs(all_modes) do
	-- Read in config.lua
	enable_cross_gcc_toolchain = true
	exec_policy_selection = nil
	local config_file_name = session_dir .. "/share/ldbox/modes/"..m_name.."/config.lua"
	do_file(config_file_name)

	-- Modename => argvmods type table
	ruletree.catalog_set("use_gcc_argvmods", m_name,
		ruletree.new_boolean(enable_cross_gcc_toolchain))

	-- Exec policy selection table
	if exec_policy_selection ~= nil then
		local ruletype
		local selector
		local function get_rule_selector(epsrule)
			-- Recycling:
			-- LB_RULETREE_FSRULE_SELECTOR_PATH               101
			-- LB_RULETREE_FSRULE_SELECTOR_PREFIX             102
			-- LB_RULETREE_FSRULE_SELECTOR_DIR                103
			if epsrule.path then
				return 101, epsrule.path
			elseif epsrule.prefix then
				return 102, epsrule.prefix
			elseif epsrule.dir then
				return 103, epsrule.dir
			else
				return 0, nil
			end
		end
		local num_rules = 0
		for i = 1, table.maxn(exec_policy_selection) do
			local epsrule = exec_policy_selection[i]
			ruletype, selector = get_rule_selector(epsrule)
			if ruletype ~= 0 then
				if type(selector) == 'table' then
					num_rules = num_rules + table.maxn(selector)
				else
					num_rules = num_rules + 1
				end
			end
		end
		local epsrule_list_index = ruletree.objectlist_create(num_rules)
		local n = 0
		for i = 1, table.maxn(exec_policy_selection) do
			local epsrule = exec_policy_selection[i]
			ruletype, selector = get_rule_selector(epsrule)
			if ruletype == 0 then
				print("-- Skipping eps rule ["..i.."]")
			else
				if type(selector) == 'table' then
					for j=1, table.maxn(selector) do
						local offs = ruletree.add_exec_policy_selection_rule_to_ruletree(
							ruletype, selector[j], epsrule.exec_policy_name,
							0)
						ruletree.objectlist_set(epsrule_list_index, n, offs)
						n = n + 1
					end
				else
					local offs = ruletree.add_exec_policy_selection_rule_to_ruletree(
						ruletype, selector, epsrule.exec_policy_name,
						0)
					ruletree.objectlist_set(epsrule_list_index, n, offs)
					n = n + 1
				end
			end
		end
		ruletree.catalog_set("exec_policy_selection", m_name,
			epsrule_list_index)
	else
		error("No exec policy selection table in "..config_file_name)
	end
end

