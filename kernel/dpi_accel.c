/**
 * Platform Driver for DPI (Deep Packet Inspection) Hardware Accelerator 
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

#include "dpi_accel.h"


/** Initialize instance-specific driver-internal data structure */
static struct DPIDriverLocal Dpi_Local;


/** Initialize of_match_table for device tree */
#ifdef CONFIG_OF
	static struct of_device_id Dpi_Driver_of_Match[] = 
	{
		{ .compatible = "xlnx,dpi-accelerator-1.00.a", },
		{ /* end of list */ },
	};
	MODULE_DEVICE_TABLE(of, Dpi_Driver_of_Match);
#else
	#define Dpi_Driver_of_Match
#endif


void dpi_reset_filter_table(void)
{
	/**
	 *  IMPORTANT: NOT FUNCTIONAL YET
	 */
	/*
	// Set device status as busy
	Dpi_Local.device_status = STATUS_BUSY;

	printk(KERN_NOTICE "Filter table on DPI Hardware is being reset\n");

	// Write filter table info and reset device
	iowrite32(5, &Dpi_Local->accel_ptr + REG_OFFSET_NUM_STATES);
	iowrite32(1, &Dpi_Local->accel_ptr + REG_OFFSET_NUM_FINALS);
	iowrite32(REG_CTRL_RST, &Dpi_Local->accel_ptr + REG_OFFSET_CTRL);
	*/
}


void dpi_push_packet_payload(char* payload, unsigned int p_size)
{
	// Set device status as busy
	Dpi_Local.device_status = STATUS_BUSY;

	// Make payload buffer accessible for DMA
	// Hold references for DMA and future unmap
	Dpi_Local.tx_bd_virt->next = Dpi_Local.tx_bd_phys;
	Dpi_Local.tx_bd_virt->len = p_size;
	Dpi_Local.tx_bd_virt->app0 = STS_CTRL_APP0_SOP | STS_CTRL_APP0_EOP;
	Dpi_Local.tx_bd_virt->phys = dma_map_single(Dpi_Local.dev->parent, payload, 
												p_size, DMA_TO_DEVICE);

	printk(KERN_DEBUG "Pushing packet payload into DPI hardware -- Addr: %08x, Size: %u\n",
		(uint32_t) Dpi_Local.tx_bd_virt->phys, Dpi_Local.tx_bd_virt->len);

	/**
	 *  IMPORTANT: ctrl mask not functional yet
	 */
	// iowrite32(REG_CTRL_FILTER, &Dpi_Local->accel_ptr + REG_OFFSET_CTRL);

	// Kick off DMA transfer for payload filtering
	Dpi_Local.dma_out(TX_TAILDESC_PTR, Dpi_Local.tx_bd_phys);
}


int dpi_get_filter_result(void)
{
	uint32_t stat_reg_val;
	int timeout = 1000;

	printk(KERN_DEBUG "Xtables is fetching the filter result from driver...\n");

	// If device is not set, quickly return error
	if(Dpi_Local.device_status == STATUS_NOT_SET)
	{
		return -1;
	}

	// If status is busy, wait until the value is loaded
	while(Dpi_Local.device_status == STATUS_BUSY)
	{
		udelay(1);
		timeout--;

		if(! timeout)
		{
			// If timeout is occurred, report the error
			printk(KERN_INFO "dpi: Timeout in fetching filter result from driver\n");

			// If timeout is occured, report current device status in debug mode
			stat_reg_val = ioread32((void*) Dpi_Local.accel_ptr + REG_OFFSET_STATUS);
			printk(KERN_DEBUG "DPI Status register at timeout: 0x%08x\n", stat_reg_val);

			return -1;
		}
	}

	// Set new device status 
	Dpi_Local.device_status = STATUS_NOT_SET;

	// Return packet result that is set
	return Dpi_Local.packet_result;
}


/** Function that evaluates device status */
static void dpi_evaluate_dev_status(void *lp, uint32_t stat_reg_val)
{
	struct DPIDriverLocal *local_ptr = (struct DPIDriverLocal *) lp;

	// If the device is busy, quickly return
	if(stat_reg_val & REG_STATUS_BUSY)
	{
		printk(KERN_INFO "Device is busy. The result of last operation cannot be fetched!\n");
		return ;
	}

	if(stat_reg_val & REG_STATUS_RST_END)
	{
		// If last finished operation is reset, update device status and quickly return
		dev_info(local_ptr->dev, "Filter table reset on DPI Hardware is completed!\n");
		Dpi_Local.device_status = STATUS_NOT_SET;
	}
	else if(stat_reg_val & REG_STATUS_FILTER_END)
	{
		// Last finished operation is filter
		// Update packet result variable according to status register
		if(stat_reg_val & REG_STATUS_ERR)
		{
			dev_info(local_ptr->dev, "Error occured in the last operation on device!\n");
			Dpi_Local.packet_result = -1;
		}
		else
		{
			Dpi_Local.packet_result = stat_reg_val & REG_STATUS_FILTER_MATCH;
			printk(KERN_DEBUG "Does DPI Accelerator match packet? : %d\n", (Dpi_Local.packet_result > 0));
		}

		// Unmap DMA reference
		dma_unmap_single(Dpi_Local.dev->parent, Dpi_Local.tx_bd_virt->phys, 
						Dpi_Local.tx_bd_virt->len, DMA_TO_DEVICE);

		// Set device status as ready to read
		Dpi_Local.device_status = STATUS_READ_READY;
	}
	else
	{
		// In unrecognized signal, update device status and allow package
		dev_info(local_ptr->dev, "Unrecognized DPI decision on packet. Allowing it! \n");
		Dpi_Local.packet_result = -1;
		Dpi_Local.device_status = STATUS_NOT_SET;		
	}
}


/** Function that handles DMA TX interrupt */
static irqreturn_t dpi_tx_interrupt(int irq, void *lp)
{
	unsigned int dma_status;
	uint32_t stat_reg_val;
	struct DPIDriverLocal *local_ptr = (struct DPIDriverLocal *) lp;

	// Get DMA IRQ status and re-write it (to inform that interrupt is received) 
	dma_status = local_ptr->dma_in(TX_IRQ_REG);
	local_ptr->dma_out(TX_IRQ_REG, dma_status);

	// Get Tx state and evaluate it
	dma_status = local_ptr->dma_in(TX_CHNL_STS);

	if (dma_status & CHNL_STS_CMPLT)
	{
		// If DMA is successful, read and evaluate device status
		stat_reg_val = ioread32((void*) local_ptr->accel_ptr + REG_OFFSET_STATUS);
		printk(KERN_DEBUG "DPI status at TX Interrupt: 0x%08x\n", stat_reg_val);

		dpi_evaluate_dev_status(local_ptr, stat_reg_val);
	}
	else if (dma_status & CHNL_STS_ERR)
	{
		// If DMA error is occured, log it and force error state
		dev_err(local_ptr->dev, "DMA transfer error 0x%x\n", dma_status);
		dpi_evaluate_dev_status(local_ptr, REG_STATUS_ERR);
	}

	return IRQ_HANDLED;
}


/** Function for DCR based DMA read */
static u32 dpi_dma_dcr_in(int reg)
{
	u32 retval;

	retval = dcr_read(Dpi_Local.sdma_dcrs, reg);

	return retval;
}


/** Function for DCR based DMA write */
static void dpi_dma_dcr_out(int reg, u32 value)
{
	dcr_write(Dpi_Local.sdma_dcrs, reg, value);
}


/** The function that setups the DCR address and I/O functions */
static int dpi_dcr_setup(struct DPIDriverLocal *lp, struct platform_device *op,
				struct device_node *np)
{
	unsigned int dcrs;

	// Setup the dcr address mapping, if it's in the device tree
	dcrs = dcr_resource_start(np, 0);
	if (dcrs) 
	{
		lp->sdma_dcrs = dcr_map(np, dcrs, dcr_resource_len(np, 0));
		lp->dma_in = dpi_dma_dcr_in;
		lp->dma_out = dpi_dma_dcr_out;
		return 0;
	}
	
	// No DCR in the device tree, indicate a failure
	return -1;
}


/** The function that resets and initalizes DMA */
static int dpi_dma_init(struct DPIDriverLocal *lp)
{
	u32 timeout;

	// Allocate the tx buffer descriptor
	// It returns a virtual address and a physical address
	lp->tx_bd_virt = dma_zalloc_coherent(lp->dev->parent, sizeof(*lp->tx_bd_virt),
					  					&lp->tx_bd_phys, GFP_KERNEL);

	// If error occurs during coherent memory allocation, return an error
	if (!lp->tx_bd_virt)
	{
		return -ENOMEM;
	}

	// Reset Local Link (DMA)
	lp->dma_out(DMA_CONTROL_REG, DMA_CONTROL_RST);
	timeout = 1000;
	while (lp->dma_in(DMA_CONTROL_REG) & DMA_CONTROL_RST) 
	{
		udelay(1);
		if (--timeout == 0) 
		{
			dev_err(lp->dev, "dpi_dma_bd_init: DMA reset timed out!!\n");
			break;
		}
	}
	dev_notice(lp->dev, "DMA 1 is disabled.\n");

	// Re-enable DMA transfer
	lp->dma_out(DMA_CONTROL_REG, DMA_TAIL_ENABLE);

	// Set Tx Channel settings
	lp->dma_out(TX_CHNL_CTRL, 0x01010000 |
				  CHNL_CTRL_IRQ_EN |
				  CHNL_CTRL_IRQ_DLY_EN |
				  CHNL_CTRL_IRQ_COAL_EN |
				  CHNL_CTRL_IRQ_IOE);

	// Set the physical address of tx buffer descriptor into DMA
	lp->dma_out(TX_CURDESC_PTR, lp->tx_bd_phys);

	dev_notice(lp->dev, "TX channel of DMA 1 is enabled.\n");

	return 0;
}


/** The function that resets DMA (and leaves it disabled) **/
static void dpi_dma_release(struct DPIDriverLocal *lp)
{
	// Reset Local Link (DMA)
	lp->dma_out(DMA_CONTROL_REG, DMA_CONTROL_RST);

	// Clear coherent memory for Tx buffer descriptor
	if (lp->tx_bd_virt)
	{
		dma_free_coherent(lp->dev->parent, sizeof(*lp->tx_bd_virt),
							lp->tx_bd_virt, lp->tx_bd_phys);
	}

	dev_notice(lp->dev, "DMA 1 is disabled.\n");
}


/** The function that probes driver after registering into kernel */
static int dpi_driver_probe (struct platform_device *p_dev) 
{
	struct device_node *np;
	struct resource *r_mem;					// IO mem resources
	struct device *dev = &p_dev->dev;		// Generic device contained in platform device	
	int retval = -EBUSY;					// Default return value -> Busy Error Code

	dev_info(dev, "Probing the DPI device... \n");

	// Set device status in driver-internal data structure
	Dpi_Local.device_status = STATUS_NOT_SET;

	// Store the device struct itself for future reference
	Dpi_Local.dev = dev;

	// Get memory region assigned to the device
	r_mem = platform_get_resource(p_dev, IORESOURCE_MEM, 0);
	if (!r_mem) 
	{
		dev_err(dev, "Error in getting memory region assigned to the device. ABORTING!\n");
		retval = -ENODEV;
		goto no_mem;
	}

	// Register memory info into driver-internal data structure
	Dpi_Local.mem_start = r_mem->start;
	Dpi_Local.mem_size = r_mem->end - r_mem->start + 1;

	// Try to validate memory region assigned to the device
	if (check_mem_region(Dpi_Local.mem_start, Dpi_Local.mem_size))
	{
		dev_err(dev, "Error in validating assigned memory region. ABORTING!\n");
		retval = -ENOMEM;
		goto no_mem;
	}

	// Perform memory remap
	request_mem_region(Dpi_Local.mem_start, Dpi_Local.mem_size, DRIVER_NAME);
	Dpi_Local.accel_ptr = (volatile unsigned int *) 
						ioremap(Dpi_Local.mem_start, Dpi_Local.mem_size);
	
	if (!Dpi_Local.accel_ptr)
	{
		dev_err(dev, "Error in re-mapping of assigned memory region. ABORTING!\n");
		retval = -ENOMEM;
		goto no_mem;
	}

	// Print re-mapping as device info
	dev_info(dev, "0x%08lx size 0x%08lx mapped to 0x%08x\n", 
		Dpi_Local.mem_start, Dpi_Local.mem_size, (unsigned int) Dpi_Local.accel_ptr);


	// Find the DMA node, map the DMA registers, and decode the DMA IRQs
	np = of_parse_phandle(dev->of_node, "llink-connected", 0);
	if (!np) 
	{
		dev_err(dev, "Could not find DMA node. ABORTING!\n");
		retval = -ENODEV;
		goto no_interrupt;
	}

	// Setup the DMA register accesses (DCR)
	if (dpi_dcr_setup(&Dpi_Local, p_dev, np)) 
	{
		dev_err(dev, "Unable to map DMA registers. ABORTING!\n");
		of_node_put(np);
		retval = -ENODEV;
		goto no_interrupt;
	}

	// Fetch DMA Tx IRQ number
	Dpi_Local.tx_irq = irq_of_parse_and_map(np, 0);

	// Finished with the DMA node; drop the node reference
	of_node_put(np);

	// Try to request interrupt from Linux 
	retval = request_irq(Dpi_Local.tx_irq, &dpi_tx_interrupt, 0, DRIVER_NAME,  &Dpi_Local);
	if (retval)
	{
		dev_err(dev, "Cannot get interrupt %d: %d. ABORTING!\n", Dpi_Local.tx_irq, retval);
		retval = -ENOMEM;
		goto no_interrupt;
	}
	
	// Reset & Initialize DMA
	retval = dpi_dma_init(&Dpi_Local);
	if(retval)
	{
		dev_err(dev, "Cannot initialize DMA buffer descriptors. ABORTING!\n");
		retval = -ENOMEM;
		goto no_dma_buffers;
	}

	// Report and return succcess
	dev_info(dev, "%s %s Initialized\n", DRIVER_NAME, DRIVER_VERSION);
	dev_info(dev, "The DPI device is successfully probed. \n");
	return 0;

	// Error handling
no_dma_buffers:
	free_irq(Dpi_Local.tx_irq, &Dpi_Local);
no_interrupt:
	iounmap( (void *) Dpi_Local.accel_ptr );
	release_mem_region(Dpi_Local.mem_start, Dpi_Local.mem_size);
no_mem:
	return retval;
}


/** The function that removes driver after unregistering from kernel */
static int dpi_driver_remove (struct platform_device *pdev) 
{
	struct device *dev = &pdev->dev;	// Generic device contained in platform device
	
	// Reset DMA 
	dpi_dma_release(&Dpi_Local);
	
	// Free assigned IRQ
	free_irq(Dpi_Local.tx_irq, &Dpi_Local);

	// Unmap and release memory
	iounmap( (void *)Dpi_Local.accel_ptr );
	release_mem_region(Dpi_Local.mem_start, Dpi_Local.mem_size);

	// Report driver remove
	dev_info(dev, "%s %s Removed\n", DRIVER_NAME, DRIVER_VERSION);
	dev_info(dev, "The DPI device is removed from kernel.\n");

	return 0;
}


/** Initialize kernel driver data structure */
static struct platform_driver Dpi_Driver = 
{
	.driver = 
	{
		.name = DRIVER_NAME,
    	.owner = THIS_MODULE,
		.of_match_table = Dpi_Driver_of_Match,
	},
	.probe          = dpi_driver_probe,
	.remove         = dpi_driver_remove,
};


int dpi_init(void)
{
	int retval = 0;

	printk(KERN_NOTICE "Trying to register the DPI Accelerator driver... \n");

	// Register platform device driver first
	retval = platform_driver_register(&Dpi_Driver);
	if (retval) 
	{
		printk(KERN_ERR "Unable to register DPI Accelerator driver... \n");
		return retval;
	}

	// Report driver load success
	printk(KERN_NOTICE "The DPI Accelerator driver is registered. \n");
	return 0;
}


void dpi_exit(void)
{
	// Unregister platform driver
	platform_driver_unregister(&Dpi_Driver);

	printk(KERN_NOTICE "The DPI Accelerator driver is unregistered.\n");
}