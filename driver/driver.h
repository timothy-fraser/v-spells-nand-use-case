// Copyright (c) 2022 Provatek, LLC.

#ifndef NAND_DRIVER_H_
#define NAND_DRIVER_H_

/* The framework should use the following timeouts as arguments to the
 * wait_ready() jump table function and the NAND_OP_WAITRDY_INSTR
 * operation instruction.  Choose the proper timeout for the operation
 * you're waiting on.  Note that these values do not match the
 * duration constants the device emulator uses; these are deliberately
 * 10% longer.
 */
#define TIMEOUT_READ_PAGE_US    110
#define TIMEOUT_WRITE_PAGE_US   660
#define TIMEOUT_ERASE_BLOCK_US 2200
#define TIMEOUT_RESET_US        550


struct nand_device *init_nand_driver(volatile unsigned long *ioregister, struct nand_device *old_dib);
struct nand_driver get_driver();


#endif
