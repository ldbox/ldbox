/*
 * Copyright (c) 2010,2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

#ifndef LB_NETWORK_H__
#define LB_NETWORK_H__

/* network/net_rules.c: */
extern int lb_map_network_addr(
	const char *binary_name,
        const char *realfnname,
        const char *protocol,
        const char *addr_type,
        const char *orig_dst_addr,
        int orig_port,
        char *result_addr_buf,
        int result_addr_buf_len,
        int *result_port);

#endif /* LB_NETWORK_H__ */
