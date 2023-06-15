// Copyright (c) 2022 Provatek, LLC.

#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>

#include "device_emu.h"
#include "driver.h"
#include "framework.h"
#include "fw_execop.h"

extern struct nand_driver driver;    /* from framework.c */

/* These constants indicate how many NAND instructions are needed in
 * an operation for each page read or programmed and for each block
 * erased.
 */

#define DATA_XFER_INSTRUCTIONS 3     /* xfer, execute, wait */
#define ERASE_INSTRUCTIONS     2     /* execute, wait */


/* instruction_count_data_xfer()
 *
 * in:     byte_addr - byte offset from start of page
 *         size      - size of transfer in bytes
 * out:    nothing
 * return: number of NAND instructions needed
 *
 * Returns the number of NAND instructions needed for the read or
 * program operation described by the input parms.
 *
 */

static unsigned int
instruction_count_data_xfer(unsigned int byte_addr,
			    unsigned int size) {

	unsigned int count = 0; /* the instruction count accumulates here */

	count++;    /* read or program setup instruction */
	count++;    /* address instruction */

	/* The device transfers data in whole pages, so we need to
	 * consider the bytes of the first page that preceed
	 * 'byte_addr' as part of our data size.  After the following
	 * adjustment, we can proceed with our calculations as if the
	 * beginning of all data transfers are page-aligned.
	 */
	size += byte_addr;

	/* Add in the instructions to handle all the whole pages. */
	count += DATA_XFER_INSTRUCTIONS * (unsigned int)(size / NUM_BYTES);

	/* Add an additional set of instructions to handle any final
	 * partial page.
	 */
	count += (size % NUM_BYTES ? DATA_XFER_INSTRUCTIONS : 0);
	
	return count;

} /* instruction_count_data_xfer() */


/* instruction_count_erase()
 *
 * in:     start_block_addr - number of first block to erase
 *         end_block_addr   - number of last block to erase
 * out:    nothing
 * return: number of NAND instructions needed
 *
 * Returns the number of NAND instructions needed for the erase operation
 * described by the input parms.
 *
 */

static unsigned int
instruction_count_erase(unsigned int start_block_addr,
			unsigned int end_block_addr) {

	unsigned int count = 0; /* the instruction count accumulates here */

	count++;    /* erase setup instruction */
	count++;    /* address instruction */

	count += ERASE_INSTRUCTIONS * ((end_block_addr - start_block_addr)
		+ 1);
	
	return count;

} /* instruction_count_erase() */

	
/* exec_write()
 *
 * in:     buffer - array of bytes to write to storage device
 *         offset - device address to receive data
 *         size   - number of bytes to write, can be multiple pages
 * out:    nothing
 * return: -1 on error (specifically, device timeout) else 0.
 *          
 * Writes size bytes from buffer to NAND storage device.  Writes them
 * to storage starting at the storage address in offset.
 *
 * This version of write works with drivers that provide the framework
 * with a command interpreter rather than a jump table.
 *
 */

int
exec_write(const unsigned char* buffer, unsigned int offset,
	unsigned int size) {
	
	unsigned int bytes_left = size;
	unsigned int cursor = 0;

	unsigned char block_addr = offset / (NUM_PAGES * NUM_BYTES);
	offset %= (NUM_PAGES * NUM_BYTES);
	unsigned char page_addr = offset / NUM_BYTES;
	unsigned char byte_addr = offset % NUM_BYTES;

	unsigned int size_to_write;

	struct nand_operation operation;

	int ret_val = 0;    /* optimistically presume success */

	operation.instrs = malloc(instruction_count_data_xfer(byte_addr,
		size) * sizeof(struct nand_op_instr));
	assert(operation.instrs != NULL);

	int i = 0;
	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION
	
	operation.instrs[i].type = NAND_OP_CMD_INSTR;
	operation.instrs[i++].ctx.cmd.opcode = C_PROGRAM_SETUP;

	operation.instrs[i].type = NAND_OP_ADDR_INSTR;
	operation.instrs[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_IO;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] = block_addr;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_PAGE ]  = page_addr;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BYTE ]  = byte_addr;
	i++;

	do {
		size_to_write = NUM_BYTES;
		if (offset != 0) {
			size_to_write = NUM_BYTES - offset;
			offset = 0;
		}
		if (bytes_left < NUM_BYTES &&
			bytes_left < size_to_write) {
			size_to_write = bytes_left;
		}

		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_DATA_IN_INSTR;
		operation.instrs[i].ctx.data_in.len = size_to_write;
		operation.instrs[i++].ctx.data_in.buf = &buffer[cursor];

		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_CMD_INSTR;
		operation.instrs[i++].ctx.cmd.opcode = C_PROGRAM_EXECUTE;

		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_WAITRDY_INSTR;
		operation.instrs[i++].ctx.waitrdy.timeout_ms =
			TIMEOUT_WRITE_PAGE_US;

		cursor += size_to_write;
		bytes_left -= size_to_write;

	} while(bytes_left > 0);

	assert(operation.ninstrs == instruction_count_data_xfer(byte_addr,
		size));
	
	if (driver.operation.exec_op(&operation)) {
		ret_val = -1;  /* timeout */
	}

	free(operation.instrs);
	return ret_val;
}


/* exec_read()
 *
 * in:     offset - read data beginning at this device address
 *         size   - number of bytes to read, can be multiple pages
 * out:    buffer - receives data read from storage device
 * return: -1 on error (specifically, device timeout) else 0.
 *          
 * Reads size bytes from NAND storage device to buffer starting at the
 * storage address in offset.
 *
 * This version of read works with drivers that provide the framework
 * with a command interpreter rather than a jump table.
 *
 */

int
exec_read(unsigned char* buffer, unsigned int offset, unsigned int size) {
	
	unsigned int bytes_left = size;
	unsigned int cursor = 0;

	unsigned char block_addr = offset / (NUM_PAGES * NUM_BYTES);
	offset %= (NUM_PAGES * NUM_BYTES);
	unsigned char page_addr = offset / NUM_BYTES;
	unsigned char byte_addr = offset % NUM_BYTES;
	
	unsigned int size_to_read;

	struct nand_operation operation;

	int ret_val = 0;    /* optimistically presume success */

	operation.instrs = malloc(instruction_count_data_xfer(byte_addr,
		size) * sizeof(struct nand_op_instr));
	assert(operation.instrs != NULL);
	
	int i = 0;
	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION

	operation.instrs[i].type = NAND_OP_CMD_INSTR;
	operation.instrs[i++].ctx.cmd.opcode = C_READ_SETUP;

	operation.instrs[i].type = NAND_OP_ADDR_INSTR;
	operation.instrs[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_IO;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] = block_addr;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_PAGE ]  = page_addr;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BYTE ]  = byte_addr;
	i++;

	do {
		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_CMD_INSTR;
		operation.instrs[i++].ctx.cmd.opcode = C_READ_EXECUTE;

		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_WAITRDY_INSTR;
		operation.instrs[i++].ctx.waitrdy.timeout_ms =
			TIMEOUT_READ_PAGE_US;
	
		size_to_read = NUM_BYTES;
		if (offset != 0) {
			size_to_read = NUM_BYTES - offset;
			offset = 0;
		}
		if (bytes_left < NUM_BYTES && bytes_left < size_to_read) {
			size_to_read = bytes_left;
		}

		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_DATA_OUT_INSTR;
		operation.instrs[i].ctx.data_out.len = size_to_read;
		operation.instrs[i++].ctx.data_out.buf = &buffer[cursor];

		cursor += size_to_read;
		bytes_left -= size_to_read;

	} while(bytes_left > 0);

	assert(operation.ninstrs == instruction_count_data_xfer(byte_addr,
		size));
	
	if (driver.operation.exec_op(&operation))
		ret_val = -1;  /* timeout */

	free(operation.instrs);
	return ret_val;
}


/* exec_erase()
 *
 * in:     offset - byte offset to the first block to erase (not the
 *                  block number).
 *         size   - number of contiguous blocks to erase in terms of
 *                  bytes (not block count).
 * out:    nothing
 * return: -1 on device timeout, otherwise 0 (presumed success).
 *
 * This function uses the operation interpreter to erase a
 * contiguous series of blocks on the device.
 *
 * Note that, like the real Linux framework, it expects callers to
 * identify the first block to erase in terms of its number of bytes
 * from the start of device storage rather than its block number.
 * Similarly, it expects callers to indicate how many blocks to erase
 * in terms of the byte length of the series rather than the number of
 * blocks in the series.
 *
 * Polite callers will take care to specify offsets that nicely hit
 * the start of a block and sizes that are a multiple of the block
 * size.  However, this function will accomodate impolite callers by
 * expanding the region to erase to cover whatever the caller
 * specifies plus a little more as needed to erase complete blocks.
 *
 */

int
exec_erase(unsigned int offset, unsigned int size) {
	
	/* Calculate the start and end block number of the contiguous
	 * region to erase.  Round down for the start and round up for
	 * the end to ensure we ask the driver to erase complete
	 * blocks.
	 */
	unsigned char start_block_addr = offset /
		(NUM_PAGES * NUM_BYTES);
	unsigned char end_block_addr = ((offset + size) / 
		(NUM_PAGES * NUM_BYTES)) + 0.5;

	struct nand_operation operation;

	int ret_val = 0;    /* optimistically presume success */

	operation.instrs = malloc(instruction_count_erase(start_block_addr,
		end_block_addr) * sizeof(struct nand_op_instr));
	assert(operation.instrs != NULL);
	
	int i = 0;
	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION

	operation.instrs[i].type = NAND_OP_CMD_INSTR;
	operation.instrs[i++].ctx.cmd.opcode = C_ERASE_SETUP;

	operation.instrs[i].type = NAND_OP_ADDR_INSTR;
	operation.instrs[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_ERASE;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] =
		start_block_addr;
	i++;

	for (int j = start_block_addr; j <= end_block_addr; j++) {
		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_CMD_INSTR;
		operation.instrs[i++].ctx.cmd.opcode = C_ERASE_EXECUTE;

		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_WAITRDY_INSTR;
		operation.instrs[i++].ctx.waitrdy.timeout_ms =
			TIMEOUT_ERASE_BLOCK_US;
	}

	assert(operation.ninstrs == instruction_count_erase(start_block_addr,
		end_block_addr));
	
	if (driver.operation.exec_op(&operation))
		ret_val = -1;  /* timeout */

	free(operation.instrs);
	return ret_val;
}
