/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * Portion Copyright (c) 2008 Nokia Corporation.
 * (symlink- and path resolution code refactored by Lauri T. Aarnio at Nokia)
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 *
 * ----------------
 *
 * Pathmapping subsystem: Public interfaces (exported to other parts of ldbox)
 *
*/

#include <mapping.h>
#include <lb.h>
#include "liblb.h"
#include "exported.h"
#include "processclock.h"

#ifdef EXTREME_DEBUGGING
#include <execinfo.h>
#endif

#include "pathmapping.h" /* get private definitions of this subsystem */

/* ========== Public interfaces to the mapping & resolution code: ========== */

static void fwd_map_path(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	uint32_t flags,
	int exec_mode,
	uint32_t fn_class,
	mapping_results_t *res)
{
	struct lbcontext *lbctx = NULL;

	(void)exec_mode; /* not used */

	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		PROCESSCLOCK(clk1)

		lbctx = get_lbcontext();

		START_PROCESSCLOCK(LB_LOGLEVEL_INFO, &clk1, "fwd_map_path");
		ldbox_map_path_internal__c_engine(lbctx, binary_name,
			func_name, virtual_path,
			flags, 0, fn_class, res, 0);
		if (res->mres_errormsg) {
			LB_LOG(LB_LOGLEVEL_NOTICE,
				"C path mapping engine failed (%s) (%s)",
				res->mres_errormsg, virtual_path);
		}
		release_lbcontext(lbctx);
		STOP_AND_REPORT_PROCESSCLOCK(LB_LOGLEVEL_INFO, &clk1, virtual_path);
	}
}

#if 0
/* map using a non-standard ruleset. Not used currently,
 * but will be useful for e.g. mapping script interpreters
 * when everything is ready for "full mapping" which
 * includes full path resolution.
*/
void custom_map_path(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	uint32_t flags,
	uint32_t fn_class,
	mapping_results_t *res,
	ruletree_object_offset_t rule_list_offset)
{
	struct lbcontext *lbctx = NULL;

	LB_LOG(LB_LOGLEVEL_DEBUG, "%s: Map %s", __func__, virtual_path);

	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
	} else {
		PROCESSCLOCK(clk1)

		lbctx = get_lbcontext();

		START_PROCESSCLOCK(LB_LOGLEVEL_INFO, &clk1, __func__);

		ldbox_map_path_internal__c_engine(lbctx, binary_name,
			func_name, virtual_path,
			flags, 0, fn_class, res, rule_list_offset);

		if (res->mres_fallback_to_lua_mapping_engine &&
		    (res->mres_fallback_to_lua_mapping_engine[0] == '#')) {
			LB_LOG(LB_LOGLEVEL_ERROR,
				"C path mapping engine failed (%s), can't fallback to Lua (%s) - %s",
				res->mres_fallback_to_lua_mapping_engine, virtual_path, __func__);
		}
	}
	STOP_AND_REPORT_PROCESSCLOCK(LB_LOGLEVEL_INFO, &clk1, virtual_path);
}
#endif

/* map an "abstract" path: Don't do full path resolution. */
char *custom_map_abstract_path(
	ruletree_object_offset_t rule_list_offs,
	const char *binary_name,
	const char *virtual_orig_path,
	const char *func_name,
	int fn_class,
	const char **new_exec_policy_p)
{
	char			*mapping_result = NULL;
	path_mapping_context_t	ctx;
	struct path_entry_list	abs_virtual_source_path_list;
	int			min_path_len;
	ruletree_object_offset_t	rule_offs;

	if (!virtual_orig_path || (*virtual_orig_path != '/')) {
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"%s: path '%s' is relative => return(NULL)",
			__func__, virtual_orig_path);
		return(NULL);
	}

	/* fill ctx */
	clear_path_mapping_context(&ctx);
	ctx.pmc_binary_name = binary_name;
	ctx.pmc_func_name = func_name;
	ctx.pmc_fn_class = fn_class;
	ctx.pmc_virtual_orig_path = virtual_orig_path;
	ctx.pmc_dont_resolve_final_symlink = 0;
	ctx.pmc_lbctx = get_lbcontext();

	split_path_to_path_list(virtual_orig_path,
		&abs_virtual_source_path_list);

	if (is_clean_path(&abs_virtual_source_path_list) != 0) {
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"%s: path '%s' is not a clean path => return(NULL)",
			__func__, virtual_orig_path);
		free_path_list(&abs_virtual_source_path_list);
		return(NULL);
	}

	rule_offs = ruletree_get_mapping_requirements(
		rule_list_offs, &ctx, &abs_virtual_source_path_list,
		&min_path_len, NULL/*call_translate_for_all_p*/,
		LB_INTERFACE_CLASS_EXEC);
	LB_LOG(LB_LOGLEVEL_DEBUG, "%s: rule_offs = %u", __func__, rule_offs);

	if (rule_offs) {
		const char 	*errormsg = NULL;
		const char	*new_exec_policy = NULL;
		int		flags;

		ctx.pmc_ruletree_offset = rule_offs;
		mapping_result = ruletree_translate_path(
			&ctx, LB_LOGLEVEL_DEBUG, virtual_orig_path, &flags,
			&new_exec_policy, &errormsg);
		if (new_exec_policy_p) *new_exec_policy_p = new_exec_policy;
		LB_LOG(LB_LOGLEVEL_DEBUG, "%s: mapping_result = %s", __func__,
			(mapping_result ? mapping_result : "NULL"));
	}
	free_path_list(&abs_virtual_source_path_list);

	return(mapping_result);
}

char *reverse_map_path(
	const path_mapping_context_t	*ctx,
	const char *abs_host_path)
{
	char *virtual_path = NULL;

	if (!abs_host_path || !ctx) {
		return(NULL);
	}

	virtual_path = ldbox_reverse_path_internal__c_engine(
		ctx, abs_host_path, 1/*drop_chroot_prefix=true*/);
	if (!virtual_path) {
		/* no answer */
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"No result for path reversing from C engine (%s)",
				abs_host_path);
	}
	return(virtual_path);
}

void ldbox_map_path_for_lbshow(
	const char *binary_name,
	const char *func_name,
	const char *virtual_path,
	mapping_results_t *res)
{
	interface_function_and_classes_t	*ifp = interface_functions_and_classes__public;
	uint32_t	fn_class = 0;

	while (ifp && ifp->fn_name) {
		if (!strcmp(func_name, ifp->fn_name)) {
			fn_class = ifp->fn_classmask;
			LB_LOG(LB_LOGLEVEL_DEBUG,
				"%s: Found func_class 0x%X (%s)",
				__func__, fn_class, func_name);
			break;
		}
		ifp++;
	}
	if (!fn_class) {
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"%s: No func_class for %s",
			__func__, func_name);
	}

	fwd_map_path(binary_name, func_name, virtual_path,
		0/*flags*/, 0/*exec_mode*/, fn_class, res);
}

void ldbox_map_path(
	const char *func_name,
	const char *virtual_path,
	uint32_t flags,
	mapping_results_t *res,
	uint32_t classmask)
{
	fwd_map_path(
		(ldbox_binary_name ? ldbox_binary_name : "UNKNOWN"),
		func_name, virtual_path,
		flags, 0/*exec_mode*/, classmask, res);
}


void ldbox_map_path_at(
	const char *func_name,
	int dirfd,
	const char *virtual_path,
	uint32_t flags,
	mapping_results_t *res,
	uint32_t classmask)
{
	const char *dirfd_path;

	if (!virtual_path) {
		res->mres_result_buf = res->mres_result_path = NULL;
		res->mres_readonly = 1;
		return;
	}

	if ((*virtual_path == '/')
#ifdef AT_FDCWD
	    || (dirfd == AT_FDCWD)
#endif
	   ) {
		/* same as ldbox_map_path() */
		fwd_map_path(
			(ldbox_binary_name ? ldbox_binary_name : "UNKNOWN"),
			func_name, virtual_path,
			flags, 0/*exec_mode*/, classmask, res);
		return;
	}

	/* relative to something else than CWD */
	dirfd_path = fdpathdb_find_path(dirfd);

	if (dirfd_path) {
		/* pathname found */
		char *virtual_abs_path_at_fd = NULL;

		if (asprintf(&virtual_abs_path_at_fd, "%s/%s", dirfd_path, virtual_path) < 0) {
			/* asprintf failed */
			abort();
		}
		LB_LOG(LB_LOGLEVEL_DEBUG,
			"Synthetic path for %s(%d,'%s') => '%s'",
			func_name, dirfd, virtual_path, virtual_abs_path_at_fd);

		fwd_map_path(
			(ldbox_binary_name ? ldbox_binary_name : "UNKNOWN"),
			func_name,
			virtual_abs_path_at_fd, flags, 0/*exec_mode*/, classmask, res);
		free(virtual_abs_path_at_fd);

		return;
	}

	/* name not found. Can't do much here, log a warning and return
	 * the original relative path. That will work if we are lucky, but
	 * not always..  */
	LB_LOG(LB_LOGLEVEL_WARNING, "Path not found for FD %d, for %s(%s)",
		dirfd, func_name, virtual_path);
	res->mres_result_buf = res->mres_result_path = strdup(virtual_path);
	res->mres_readonly = 0;
}

/* this maps the path and then leaves "rule" and "exec_policy" to the stack, 
 * because exec post-processing needs them
*/
void ldbox_map_path_for_exec(
	const char *func_name,
	const char *virtual_path,
	mapping_results_t *res)
{
	fwd_map_path(
		(ldbox_binary_name ? ldbox_binary_name : "UNKNOWN"),
		func_name,
		virtual_path, 0/*flags*/, 1/*exec mode*/,
		LB_INTERFACE_CLASS_EXEC, res);
}

char *scratchbox_reverse_path(
	const char *func_name,
	const char *abs_host_path,
	uint32_t classmask)
{
	char *virtual_path;
	path_mapping_context_t	ctx;

	clear_path_mapping_context(&ctx);
	ctx.pmc_binary_name = (ldbox_binary_name ? ldbox_binary_name : "UNKNOWN");
	ctx.pmc_func_name = func_name;
	ctx.pmc_fn_class = classmask;
	ctx.pmc_virtual_orig_path = "";
	ctx.pmc_dont_resolve_final_symlink = 0;
	ctx.pmc_lbctx = get_lbcontext();

	virtual_path = reverse_map_path(&ctx, abs_host_path);
	release_lbcontext(ctx.pmc_lbctx);
	return(virtual_path);
}

void	free_mapping_results(mapping_results_t *res)
{
	if (res->mres_result_buf) free(res->mres_result_buf);
	if (res->mres_result_path_was_allocated && res->mres_result_path)
		free(res->mres_result_path);
	if (res->mres_virtual_cwd) free(res->mres_virtual_cwd);
	if (res->mres_allocated_exec_policy_name) free(res->mres_allocated_exec_policy_name);
	/* res->mres_error_text is a constant string, and not freed, ever */
	clear_mapping_results_struct(res);
}

/* manually force the result; used by _nomap versions of GATEs */
void	force_path_to_mapping_result(mapping_results_t *res, const char *path)
{
	res->mres_result_path = res->mres_result_buf = strdup(path);
	res->mres_result_path_was_allocated = 0;
	res->mres_virtual_cwd = NULL;
	res->mres_errno = 0;
}

