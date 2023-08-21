// Copyright (c) 2022 Provatek, LLC.

#ifndef NAND_DRIVER_H_
#define NAND_DRIVER_H_

/* Drivers should sleep this many microseconds between each poll of
 * the device's readiness status.
 */
#define NAND_POLL_INTERVAL_US 25

/* The framework should use the following timeouts as arguments to the
 * wait_ready() jump table function and the NAND_OP_WAITRDY_INSTR
 * operation instruction.  Choose the proper timeout for the operation
 * you're waiting on.
 *
 */

#define TIMEOUT_READ_PAGE_US   (READ_PAGE_DURATION + NAND_POLL_INTERVAL_US)
#define TIMEOUT_WRITE_PAGE_US  (WRITE_PAGE_DURATION + NAND_POLL_INTERVAL_US)
#define TIMEOUT_ERASE_BLOCK_US (ERASE_BLOCK_DURATION + NAND_POLL_INTERVAL_US)
#define TIMEOUT_RESET_US       (RESET_DURATION + NAND_POLL_INTERVAL_US)


struct nand_device *init_nand_driver(volatile unsigned long *ioregister,
	struct nand_device *old_dib);
struct nand_driver get_driver();


#endif
