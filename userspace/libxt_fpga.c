/**
 * Shared library add-on for Iptables to support FPGA matching.
 * Written by Engin Ertas <engin.ertas@ceng.metu.edu.tr>
 */

#include "xt_fpga.h"


static void fpga_help(void)
{
	printf(
		"fpga match options:\n"
		"--filter      				Enables filter for matching packets\n"
		"--print 					Enables logging for matching packets\n"
	);
}


static void fpga_init(struct xt_entry_match *m)
{
	printf("** FPGA rule is being initialized... \n");




	printf("\tInitialization is completed. \n");
}


static int fpga_parse(int c, char **argv, int invert, unsigned int *flags,
             const void *entry, struct xt_entry_match **match)
{
	struct xt_fpga_info *shared_info = (struct xt_fpga_info *)(*match)->data;
	printf("** Parsing FPGA rule arguments... \n");

	switch (c) 
	{
		case '1':
			printf("\tFilter is enabled. \n");
			shared_info->filter_enabled = 1;
			break;

		case '2':
			printf("\tPacket info logging is enabled. \n");
			shared_info->print_enabled = 1;
			break;

		default:
			return 0;
	}

	return 1;
}


static void fpga_final_check(unsigned int flags)
{
	printf("** Final check is made for entered FPGA rule.\n");
}


void _init(void)
{
	printf("** Userspace shared library for Xtables is loaded.\n");
	xtables_register_match(&fpga_mt_reg[0]);
	xtables_register_match(&fpga_mt_reg[1]);
}