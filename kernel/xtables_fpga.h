#ifndef _XTABLES_FPGA_H
#define _XTABLES_FPGA_H

/**
 * FPGA-Based String Match Module for Xtables
 *
 * Written by Engin Ertas <engin.ertas@ceng.metu.edu.tr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/netfilter/x_tables.h>
#include "dpi_accel.h"

#define PERR(fmt, args...) printk(KERN_ERR "xt_fpga: " fmt, ## args)
#define PNOTICE(fmt, args...) printk(KERN_NOTICE "xt_fpga: " fmt, ## args)
#define PINFO(fmt, args...) printk(KERN_INFO "xt_fpga: " fmt, ## args)

MODULE_AUTHOR("Engin Ertas <engin.ertas@ceng.metu.edu.tr>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Xtables: FPGA-Based String Matching");
MODULE_ALIAS("ipt_fpga");
MODULE_ALIAS("ip6t_fpga");

/** Packet-specific filter info */
struct xt_fpga_info 
{
	bool filter_enabled;
	bool print_enabled;
};

/** 
 *	This function checks if the filter matches given payload via DPI hardware
 *		returns 1 if the filter matches the packet payload 
 * 		returns 0 otherwise
 */
static bool matches(char *, unsigned int);

/**
 *  This function is called when a packet is received. 
 *		returns true to filter the packet, 
 *		returns false to allow it to pass
 */
static bool fpga_mt(const struct sk_buff *, struct xt_action_param *);

/** called when a rule including fpga module is added into an iptables chain */
static int fpga_mt_check(const struct xt_mtchk_param *);

/** called when a rule including fpga module is removed from the iptables chain */
static void fpga_mt_destroy(const struct xt_mtdtor_param *);

/** The function that initializes module */
static int __init fpga_mt_init(void);

/** The function that unloads module */
static void __exit fpga_mt_exit(void);

/** Xtables entry struct for this module */
static struct xt_match xt_fpga_mt_reg[] __read_mostly = 
{
	{
		.name 		= "fpga",
		.revision	= 0,
		.family		= NFPROTO_UNSPEC,
		.checkentry	= fpga_mt_check,
		.match 		= fpga_mt,
		.destroy 	= fpga_mt_destroy,
		.matchsize	= sizeof(struct xt_fpga_info),
		.me 		= THIS_MODULE
	},
	{
		.name 		= "fpga",
		.revision	= 1,
		.family		= NFPROTO_UNSPEC,
		.checkentry	= fpga_mt_check,
		.match 		= fpga_mt,
		.destroy 	= fpga_mt_destroy,
		.matchsize	= sizeof(struct xt_fpga_info),
		.me 		= THIS_MODULE
	},
};

#endif