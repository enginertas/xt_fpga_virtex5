#ifndef _XT_FPGA_H
#define _XT_FPGA_H

/**
 * Shared library add-on for Iptables to support FPGA matching.
 * Written by Engin Ertas <engin.ertas@ceng.metu.edu.tr>
 */

#include <stdio.h>
#include <xtables.h>
#include <getopt.h>

/** Packet-specific filter info */
struct xt_fpga_info 
{
	bool filter_enabled;
	bool print_enabled;
};

/** The function that prints the fpga match arguments */
static void fpga_help(void);

/** The function that initializes the shared fpga info struct. */
static void fpga_init(struct xt_entry_match *);

/** The function that parses the arguments of fpga match */
static int fpga_parse(int c, char **, int, unsigned int *,
					const void *, struct xt_entry_match **);

/**
 * It is called after fpga_init is returned.  
 * It is final check mechanism
 */
static void fpga_final_check(unsigned int);

/** The option struct for iptables rule arguments */
static const struct option fpga_opts[] = 
{
	{ "filter", 0, NULL, '1' },
	{ "print", 0, NULL, '2' },
	{ .name = NULL }
};

/** Userspace Xtables entry for FPGA matcher */
static struct xtables_match fpga_mt_reg[] = 
{
	{
		.name          = "fpga",
		.revision      = 0,
		.family        = NFPROTO_UNSPEC,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_fpga_info)),
		.help          = fpga_help,
		.init          = fpga_init,
		.parse         = fpga_parse,
		.final_check   = fpga_final_check,
		.extra_opts    = fpga_opts,
	},
	{
		.name          = "fpga",
		.revision      = 1,
		.family        = NFPROTO_UNSPEC,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_fpga_info)),
		.help          = fpga_help,
		.init          = fpga_init,
		.parse         = fpga_parse,
		.final_check   = fpga_final_check,
		.extra_opts    = fpga_opts,
	},
};

/** Called when FPGA Xtables module is loaded */
void _init(void);

#endif