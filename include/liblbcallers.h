#ifndef LIBLBCALLERS_H__
#define LIBLBCALLERS_H__

/* liblbcallers: Macros for creating functions for calling
 * routines from liblb, which may not be available.
 *
 * Copyright (c) 2008 Nokia Corporation. All rights reserved.
 * Author: Lauri T. Aarnio
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

extern void *liblb_handle;

/* create a wrapper to a function returning void */
#define LIBLB_VOID_CALLER(funct_name, param_list, param_names) \
	static void call_ ## funct_name param_list \
	{ \
		static	void *fnptr = NULL; \
		if (!fnptr && liblb_handle) \
			fnptr = dlsym(liblb_handle, #funct_name); \
		if (fnptr) { \
			((void(*)param_list)fnptr)param_names; \
			return; \
		} \
	} \
	extern void funct_name param_list; /* ensure that we got the prototype right */

/* create a wrapper to a function with returns something */
#define LIBLB_CALLER(return_type, funct_name, param_list, param_names, errorvalue) \
	static return_type call_ ## funct_name param_list \
	{ \
		static	void *fnptr = NULL; \
		if (!fnptr && liblb_handle) \
			fnptr = dlsym(liblb_handle, #funct_name); \
		if (fnptr) { \
			return(((return_type(*)param_list)fnptr)param_names); \
		} \
		return(errorvalue); \
	} \
	extern return_type funct_name param_list; /* ensure that we got the prototype right */

#endif /* LIBLBCALLERS_H__ */
