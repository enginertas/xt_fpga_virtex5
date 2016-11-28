#ifndef _DPI_ACCEL_H
#define _DPI_ACCEL_H

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

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/dcr.h>
#include <asm/dcr-regs.h>

/** Driver property macros */
#define DRIVER_NAME 					"dpi"
#define DRIVER_VERSION 					"1.0"

/** Status macros */
#define STATUS_NOT_SET					0 		// Not set yet
#define STATUS_BUSY						1 		// Processing data
#define STATUS_READ_READY				2 		// There is a result to read

/** Hardware Accelerator Registers macros */
#define REG_OFFSET_CTRL 				0x00
#define REG_CTRL_RST               		(1<<1)
#define REG_CTRL_FILTER            		(1<<2)

#define REG_OFFSET_STATUS				0x04
#define REG_STATUS_BUSY             	(1<<7)
#define REG_STATUS_ERR					(1<<6)
#define REG_STATUS_RST_END				(1<<5)
#define REG_STATUS_FILTER_END			(1<<4)
#define REG_STATUS_FILTER_MATCH			(1<<3)

#define REG_OFFSET_NUM_STATES			0x08
#define REG_OFFSET_NUM_FINALS			0x0c

/** DCR (Device Control Register) Macros for SDMA */
#define TX_NXTDESC_PTR      			0x00	// r
#define TX_CURBUF_ADDR      			0x01	// r
#define TX_CURBUF_LENGTH    			0x02	// r
#define TX_CURDESC_PTR     				0x03	// rw
#define TX_TAILDESC_PTR     			0x04	// rw

/*
 TX Control register bit definitions
 0:7      24:31       IRQTimeout
 8:15     16:23       IRQCount
 16:20    11:15       Reserved
 21       10          0
 22       9           UseIntOnEnd
 23       8           LdIRQCnt
 24       7           IRQEn
 25:28    3:6         Reserved
 29       2           IrqErrEn
 30       1           IrqDlyEn
 31       0           IrqCoalEn
*/
#define TX_CHNL_CTRL        			0x05	// rw
#define CHNL_CTRL_IRQ_IOE       		(1 << 9)
#define CHNL_CTRL_IRQ_EN        		(1 << 7)
#define CHNL_CTRL_IRQ_ERR_EN    		(1 << 2)
#define CHNL_CTRL_IRQ_DLY_EN    		(1 << 1)
#define CHNL_CTRL_IRQ_COAL_EN   		(1 << 0)

/*
 TX IRQ register bit definitions
 0:7      24:31       DltTmrValue
 8:15     16:23       ClscCntrValue
 16:17    14:15       Reserved
 18:21    10:13       ClscCnt
 22:23    8:9         DlyCnt
 24:28    3::7        Reserved
 29       2           ErrIrq
 30       1           DlyIrq
 31       0           CoalIrq
 */
#define TX_IRQ_REG          			0x06	// rw

/*
 TX Status register bit definitions
 0:9      22:31   Reserved
 10       21      TailPErr
 11       20      CmpErr
 12       19      AddrErr
 13       18      NxtPErr
 14       17      CurPErr
 15       16      BsyWr
 16:23    8:15    Reserved
 24       7       Error
 25       6       IOE
 26       5       SOE
 27       4       Cmplt
 28       3       SOP
 29       2       EOP
 30       1       EngBusy
 31       0       Reserved
*/
#define TX_CHNL_STS         			0x07	// r
#define CHNL_STS_CMPLT					(1 << 4)
#define CHNL_STS_ERR					(1 << 7)

/*
 DMA control register bit definitions
 2 		DMA Tail Transfer enabled
 0 		DMA Reset
 */
#define DMA_CONTROL_REG             	0x10    // rw
#define DMA_CONTROL_RST                 (1 << 0)
#define DMA_TAIL_ENABLE                 (1 << 2)

/** CDMAC descriptor status bit definitions */
#define STS_CTRL_APP0_ERR         (1 << 31)
#define STS_CTRL_APP0_IRQONEND    (1 << 30)
#define STS_CTRL_APP0_STOPONEND   (1 << 29)
#define STS_CTRL_APP0_CMPLT       (1 << 28)
#define STS_CTRL_APP0_SOP         (1 << 27)
#define STS_CTRL_APP0_EOP         (1 << 26)
#define STS_CTRL_APP0_ENGBUSY     (1 << 25)
#define STS_CTRL_APP0_ENGRST      (1 << 24)

/** Buffer descriptor data structure for DMA */
struct cdmac_bd 
{
	u32 next;	/* Physical address of next buffer descriptor */
	u32 phys;
	u32 len;
	u32 app0;
	u32 app1;	/* TX start << 16 | insert */
	u32 app2;	/* TX csum */
	u32 app3;
	u32 app4;	/* skb for TX length for RX */
};

/** Instance-specific driver-internal data structure */
struct DPIDriverLocal
{
	// Pointer to platform device itself (self-reference)
	struct device *dev;

	// IRQ for DMA TX results
	int tx_irq;

	// Memory Info
	unsigned long mem_start;
	unsigned long mem_size;
	volatile unsigned int *accel_ptr;

	// Device status
	unsigned int device_status;
	int packet_result;

	// DMA buffer descriptors
	struct cdmac_bd *tx_bd_virt;
	dma_addr_t tx_bd_phys;

	// DCR (Device Control Register) Attributes for DMA Management
	dcr_host_t sdma_dcrs;
	u32 (*dma_in)(int);
	void (*dma_out)(int, u32);
};

/** The function that resets filter table on accelerator */
void dpi_reset_filter_table(void);

/** The function that pushes packet payload for filtering */
void dpi_push_packet_payload(char *, unsigned int);

/** The function that gets filter result of payload */
int dpi_get_filter_result(void);

/** The function that registers driver into kernel */
int dpi_init(void);

/** The function that deregisters driver from kernel */
void dpi_exit(void);

#endif