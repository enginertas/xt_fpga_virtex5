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

#include "xtables_fpga.h"


static bool matches(char *payload, unsigned int p_len)
{
	// Push packet payload into DPI hardware
	dpi_push_packet_payload(payload, p_len);

	// Get and return filter result from DPI hardware
	return (dpi_get_filter_result() > 0);
}


static bool fpga_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	int result;
	const struct xt_fpga_info *conf;

	// Get rule info for given packet
	conf = (const struct xt_fpga_info *) (par->matchinfo);
	
	// Check if packet payload matches with filter
	result = matches(skb->data, (skb->len - skb->data_len));

	if(result)
	{
		if(conf->print_enabled)
		{
			PINFO("Packet payload matches with filter.\n");
		}

		// When payload matches
		// return true if filter is enabled, return false otherwise
		return conf->filter_enabled;
	}
	else 
	{
		// If payload does not match, quickly return false
		return false;
	}
}


static int fpga_mt_check(const struct xt_mtchk_param *par)
{
	struct xt_fpga_info *conf;

	// Get rule info
	conf = (struct xt_fpga_info *) par->matchinfo;

	// Report rule load
	PNOTICE("Appending/Inserting an fpga matcher rule into iptables... \n");

	// Reset Filter Table(FSM) in DPI Accelerator
	dpi_reset_filter_table();

	// Report rule settings
	PINFO("is status enabled? : %d\n", (int) conf->print_enabled);
	PINFO("is filter enabled? : %d\n", (int) conf->filter_enabled);

	return 0;
}


static void fpga_mt_destroy(const struct xt_mtdtor_param *par)
{
	PNOTICE("Removing an fpga matcher rule from iptables... \n");
}


static int __init fpga_mt_init(void)
{
	int retval;

	PNOTICE("FPGA matcher for Xtables is being loaded...\n");
	
	// Try to initialize DPI driver. If it fails, return with error
	retval = dpi_init();
	if(retval)
	{
		PERR("Loading FPGA matcher failed due to DPI init error!\n");
		return retval; 
	}

	// Try to register this module into Xtables. If it fails, unload DPI driver
	retval = xt_register_matches(xt_fpga_mt_reg, ARRAY_SIZE(xt_fpga_mt_reg));
	if(retval)
	{
		PERR("FPGA matcher registration into Xtables is failed. Unloading DPI driver...\n");
		dpi_exit();
	}
	else
	{
		PNOTICE("FPGA matcher is successfully loaded.\n");
	}

	// Return success state
	return retval;
}

static void __exit fpga_mt_exit(void)
{
	// Firstly, unregister Xtables FPGA matcher
	xt_unregister_matches(xt_fpga_mt_reg, ARRAY_SIZE(xt_fpga_mt_reg));
	PNOTICE("Xtables FPGA matcher is unloaded\n");

	// Secondly, unload DPI driver
	dpi_exit();
}

module_init(fpga_mt_init);
module_exit(fpga_mt_exit);