// Copyright (c) 2022 Provatek, LLC.

#include <sys/types.h>

#include "device_emu.h"
#include "driver.h"
#include "framework.h"
#include "fw_execop.h"

extern struct nand_driver driver;  /* from framework.c */


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
	struct nand_op_instr instructions[4096];
	
	int i = 0;
	operation.ninstrs = 0;
	operation.instrs = instructions;

	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION

	instructions[i].type = NAND_OP_CMD_INSTR;
	instructions[i++].ctx.cmd.opcode = C_PROGRAM_SETUP;

	instructions[i].type = NAND_OP_ADDR_INSTR;
	instructions[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_IO;
	instructions[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] = block_addr;
	instructions[i].ctx.addr.addrs[ NAND_INSTR_PAGE ]  = page_addr;
	instructions[i].ctx.addr.addrs[ NAND_INSTR_BYTE ]  = byte_addr;
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
		instructions[i].type = NAND_OP_DATA_IN_INSTR;
		instructions[i].ctx.data_in.len = size_to_write;
		instructions[i++].ctx.data_in.buf = &buffer[cursor];

		operation.ninstrs++;
		instructions[i].type = NAND_OP_CMD_INSTR;
		instructions[i++].ctx.cmd.opcode = C_PROGRAM_EXECUTE;

		operation.ninstrs++;
		instructions[i].type = NAND_OP_WAITRDY_INSTR;
		instructions[i++].ctx.waitrdy.timeout_ms =
			TIMEOUT_WRITE_PAGE_US;

		cursor += size_to_write;
		bytes_left -= size_to_write;

	} while(bytes_left > 0);

	if (driver.operation.exec_op(&operation))
		return -1;  /* timeout */

	return 0;
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
	struct nand_op_instr instructions[4096];
	
	int i = 0;
	operation.ninstrs = 0;
	operation.instrs = instructions;

	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION

	instructions[i].type = NAND_OP_CMD_INSTR;
	instructions[i++].ctx.cmd.opcode = C_READ_SETUP;

	instructions[i].type = NAND_OP_ADDR_INSTR;
	instructions[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_IO;
	instructions[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] = block_addr;
	instructions[i].ctx.addr.addrs[ NAND_INSTR_PAGE ]  = page_addr;
	instructions[i].ctx.addr.addrs[ NAND_INSTR_BYTE ]  = byte_addr;
	i++;

	do {
		operation.ninstrs++;
		instructions[i].type = NAND_OP_CMD_INSTR;
		instructions[i++].ctx.cmd.opcode = C_READ_EXECUTE;

		operation.ninstrs++;
		instructions[i].type = NAND_OP_WAITRDY_INSTR;
		instructions[i++].ctx.waitrdy.timeout_ms =
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
		instructions[i].type = NAND_OP_DATA_OUT_INSTR;
		instructions[i].ctx.data_out.len = size_to_read;
		instructions[i++].ctx.data_out.buf = &buffer[cursor];

		cursor += size_to_read;
		bytes_left -= size_to_read;

	} while(bytes_left > 0);

	if (driver.operation.exec_op(&operation))
		return -1;  /* timeout */

	return 0;
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
	struct nand_op_instr instructions[4096];
	
	int i = 0;
	operation.ninstrs = 0;
	operation.instrs = instructions;

	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION

	instructions[i].type = NAND_OP_CMD_INSTR;
	instructions[i++].ctx.cmd.opcode = C_ERASE_SETUP;

	instructions[i].type = NAND_OP_ADDR_INSTR;
	instructions[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_ERASE;
	instructions[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] = start_block_addr;
	i++;

	for (int j = start_block_addr; j <= end_block_addr; j++) {
		operation.ninstrs++;
		instructions[i].type = NAND_OP_CMD_INSTR;
		instructions[i++].ctx.cmd.opcode = C_ERASE_EXECUTE;

		operation.ninstrs++;
		instructions[i].type = NAND_OP_WAITRDY_INSTR;
		instructions[i++].ctx.waitrdy.timeout_ms =
			TIMEOUT_ERASE_BLOCK_US;
	}

	if (driver.operation.exec_op(&operation))
		return -1;  /* timeout */

	return 0;
}
