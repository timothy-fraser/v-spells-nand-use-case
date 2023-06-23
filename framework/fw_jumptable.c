// Copyright (c) 2022 Provatek, LLC.

#include <sys/types.h>
#ifdef DIAGNOSTICS
#include <stdio.h>
#endif
#include "device_emu.h"
#include "driver.h"
#include "framework.h"
#include "fw_jumptable.h"

extern struct nand_driver driver;  /* from framework.c */


/* jt_write()
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
 * with a jump table of functions rather than a command interpreter.
 *
 */

int
jt_write(unsigned char *buffer, unsigned int offset, unsigned int size) {
	
	unsigned int bytes_per_page = NUM_BYTES;
	unsigned int pages_per_block = NUM_PAGES;
	unsigned int blocks_per_chip = NUM_BLOCKS;

	unsigned int bytes_left = size;
	unsigned int cursor = 0;

	unsigned char byte_addr = offset % bytes_per_page;
	unsigned char page_addr = offset / pages_per_block;
	unsigned char block_addr = offset /
		(pages_per_block * blocks_per_chip);

	unsigned int size_to_write;

	driver.operation.jump_table.set_register(IOREG_COMMAND, 
		C_PROGRAM_SETUP);
	driver.operation.jump_table.set_register(IOREG_ADDRESS, block_addr);
	driver.operation.jump_table.set_register(IOREG_ADDRESS, page_addr);
	driver.operation.jump_table.set_register(IOREG_ADDRESS, byte_addr);

	while (bytes_left) {
		size_to_write = bytes_per_page;
		if (byte_addr != 0) {
			size_to_write = bytes_per_page - byte_addr;
			byte_addr = 0;
		}
		if (bytes_left < bytes_per_page &&
			bytes_left < size_to_write) {
			size_to_write = bytes_left;
		}

#ifdef DIAGNOSTICS
		printf("Debug: jt_write() writing %u of %u bytes to device "
		       "from buffer offset 0x%08x.\n",
		       size_to_write, size, cursor);
#endif
		
		driver.operation.jump_table.write_buffer(&buffer[cursor], 
			size_to_write);
		driver.operation.jump_table.set_register(IOREG_COMMAND, 
			C_PROGRAM_EXECUTE);
		if (driver.operation.jump_table.wait_ready(
			TIMEOUT_WRITE_PAGE_US))
			return -1;

		cursor += size_to_write;
		bytes_left -= size_to_write;
	}
	return 0;
}


/* jt_read()
 *
 * in:     offset - read data from this device address
 *         size   - number of bytes to read, can be multiple pages
 * out:    buffer - array to receive bytes read from device.
 * return: -1 on error (specifically, device timeout) else 0.
 *          
 * Reads size bytes from NAND storage device to buffer starting at
 * the storage address in offset.
 *
 * This version of read works with drivers that provide the framework
 * with a jump table of functions rather than a command interpreter.
 *
 */

int
jt_read(unsigned char *buffer, unsigned int offset, unsigned int size) {
	
	unsigned int bytes_per_page = NUM_BYTES;
	unsigned int pages_per_block = NUM_PAGES;
	unsigned int blocks_per_chip = NUM_BLOCKS;

	unsigned int bytes_left = size;
	unsigned int cursor = 0;

	unsigned char byte_addr = offset % bytes_per_page;
	unsigned char page_addr = offset / pages_per_block;
	unsigned char block_addr = offset / (pages_per_block *
		blocks_per_chip);

	unsigned int size_to_read;

	driver.operation.jump_table.set_register(IOREG_COMMAND, 
		C_READ_SETUP);
	driver.operation.jump_table.set_register(IOREG_ADDRESS, block_addr);
	driver.operation.jump_table.set_register(IOREG_ADDRESS, page_addr);
	driver.operation.jump_table.set_register(IOREG_ADDRESS, byte_addr);
	while(bytes_left) {
		driver.operation.jump_table.set_register(IOREG_COMMAND, 
			C_READ_EXECUTE);
		if (driver.operation.jump_table.wait_ready(
			TIMEOUT_READ_PAGE_US))
			return -1;  /* timeout */
			
		size_to_read = bytes_per_page;
		if (byte_addr != 0) {
			size_to_read = bytes_per_page - byte_addr;
			byte_addr = 0;
		}
		if (bytes_left < bytes_per_page && bytes_left < size_to_read) {
			size_to_read = bytes_left;
		}

#ifdef DIAGNOSTICS
		printf("Debug: jt_read() reading %u of %u bytes to device "
		       "from buffer offset 0x%08x.\n",
		       size_to_read, size, cursor);
#endif

		driver.operation.jump_table.read_buffer(&buffer[cursor], 
			size_to_read);

		cursor += size_to_read;
		bytes_left -= size_to_read;
	}
	return 0;
}


/* jt_erase()
 *
 * in:     offset - byte offset to the first block to erase (not the
 *                  block number).
 *         size   - number of contiguous blocks to erase in terms of
 *                  bytes (not block count).
 * out:    nothing
 * return: -1 on device timeout, otherwise 0 (presumed success).
 *
 * This function uses driver jump table functions to erase a
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
jt_erase(unsigned int offset, unsigned int size) {

	/* Calculate the start and end block number of the contiguous
	 * region to erase.  Round down for the start and round up for
	 * the end to ensure we ask the driver to erase complete
	 * blocks.
	 */
	unsigned char start_block_addr = offset /
		(NUM_PAGES * NUM_BYTES);
	unsigned char end_block_addr = ((offset + size) / 
		(NUM_PAGES * NUM_BYTES)) + 0.5;

	driver.operation.jump_table.set_register(IOREG_COMMAND, 
		C_ERASE_SETUP);
	driver.operation.jump_table.set_register(IOREG_ADDRESS, 
		start_block_addr);
	for (int i = start_block_addr; i <= end_block_addr; i++) {
		driver.operation.jump_table.set_register(IOREG_COMMAND, 
			C_ERASE_EXECUTE);
		if (driver.operation.jump_table.wait_ready(
			TIMEOUT_ERASE_BLOCK_US))
			return -1;  /* timeout */
	}
	return 0;
	
} /* jt_erase() */
