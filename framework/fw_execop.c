// Copyright (c) 2022 Provatek, LLC.

#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>

#ifdef DIAGNOSTICS
#include <stdio.h>
#endif

#include "device_emu.h"
#include "driver.h"
#include "framework.h"
#include "fw_execop.h"

#define PAGE_SIZE  NUM_BYTES
#define BLOCK_SIZE (NUM_PAGES * PAGE_SIZE) /* device block size in bytes */

/* The following three macros return the number of the block, the
 * number of the page within its block, and the number of the byte
 * within its page of the offset o, respectively.
 */
#define BLOCK(o)        ((o) / BLOCK_SIZE)
#define PAGE(o)         (((o) % BLOCK_SIZE) / PAGE_SIZE)
#define BYTE(o)         ((o) % PAGE_SIZE)

/* These constants indicate how many NAND instructions are needed in
 * an operation for each page read or programmed and for each block
 * erased.
 */

#define DATA_XFER_INSTRUCTIONS 3     /* xfer, execute, wait */
#define ERASE_INSTRUCTIONS     2     /* execute, wait */

extern struct nand_driver driver;    /* from framework.c */


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
 * in:     num_blocks  - number of blocks to erase
 * out:    nothing
 * return: number of NAND instructions needed
 *
 * Returns the number of NAND instructions needed for the erase operation
 * described by the input parms.
 *
 */

static unsigned int
instruction_count_erase(unsigned int num_blocks) {

	unsigned int count = 0; /* the instruction count accumulates here */

	count++;    /* erase setup instruction */
	count++;    /* address instruction */

	count += ERASE_INSTRUCTIONS * num_blocks;
	
	return count;

} /* instruction_count_erase() */


#ifdef DIAGNOSTICS

static void
print_operation(struct nand_operation *op) {

	unsigned int i;                /* counts instructions in operation */
	struct nand_op_instr *p_instr; /* points to each instruction */
	
	printf("Operation (%u): ", op->ninstrs);
	
	for (i = 0; i < op->ninstrs; i++) {
		p_instr = &(op->instrs[ i ]);
		switch (p_instr->type) {
			
		case NAND_OP_CMD_INSTR:
			printf("CMD 0x%02x ", p_instr->ctx.cmd.opcode);
			break;

		case NAND_OP_ADDR_INSTR:
			switch (p_instr->ctx.addr.naddrs) {
			case NAND_INSTR_NUM_ADDR_IO:
				printf("XADDR 0x%02x%02x%02x ",
					p_instr->ctx.addr.addrs[
					NAND_INSTR_BLOCK ],
					p_instr->ctx.addr.addrs[
					NAND_INSTR_PAGE ],
					p_instr->ctx.addr.addrs[
					NAND_INSTR_BYTE ]);
				break;
			case NAND_INSTR_NUM_ADDR_ERASE:
				printf("EADDR 0x%02x ",
					p_instr->ctx.addr.addrs[
					NAND_INSTR_BLOCK ]);
				break;
			default:
				assert(0);  /* bad number of addrs */
			}
			break;

		case NAND_OP_DATA_IN_INSTR:
			printf("DIN(W) 0x%02x ", p_instr->ctx.data_in.len);
			break;
			
		case NAND_OP_DATA_OUT_INSTR:
			printf("DOUT(R) 0x%02x ", p_instr->ctx.data_out.len);
			break;

		case NAND_OP_WAITRDY_INSTR:
			printf("WAIT %u ", p_instr->ctx.waitrdy.timeout_ms);
			break;

		default:
			assert(0);  /* bad instruction type */

		} /* switch on instruction type */
		
	} /* for all instructions in operation */

	printf("\n");
	
} /* print_operation() */

#endif
	
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

	struct nand_operation operation; /* the NAND operation to send */
	unsigned int i = 0;              /* counts instructions in operation */
	unsigned int cursor = 0;         /* index into buffer parm */
	unsigned int size_remaining;     /* bytes left to transfer */
	unsigned int available;          /* space available in current page */
	unsigned int size_this_page;     /* bytes xferred in current page */
	int ret_val = 0;                 /* optimistically presume success */

#ifdef DIAGNOSTICS
	printf("Framework exec_write() start addr 0x%08x "
		"size 0x%08x instruction count 0x%08x.\n",
	       offset, size, instruction_count_data_xfer(BYTE(offset), size));
#endif 
	
	operation.instrs = malloc(instruction_count_data_xfer(BYTE(offset),
		size) * sizeof(struct nand_op_instr));
	assert(operation.instrs != NULL);

	/* Begin with a C_PROGRAM_SETUP. */
	operation.instrs[i].type = NAND_OP_CMD_INSTR;
	operation.instrs[i].ctx.cmd.opcode = C_PROGRAM_SETUP;
	i++;

	/* Then specify the address. */
	operation.instrs[i].type = NAND_OP_ADDR_INSTR;
	operation.instrs[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_IO;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] = BLOCK(offset);
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_PAGE ]  = PAGE(offset);
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BYTE ]  = BYTE(offset);
	i++;

	/* The driver expects to transfer the data one page at a time.
	 * The first page is special: the data must begin at
	 * BYTE(offset), and if BYTE(offset) is not 0 (that is, if the
	 * beginning of the transfer is not page-aligned) the first
	 * page won't be able to hold a full PAGE_SIZE of bytes.  All
	 * subsequent pages will be able to hold up to PAGE_SIZE
	 * bytes.  Use 'available' to manage this first
	 * page/subsequent pages capacity logic. Add a transfer,
	 * execute, waitrdy trio for each page to the operation.
	 */
	size_remaining = size;
	available = PAGE_SIZE - BYTE(offset);  /* First page capacity. */
	while (size_remaining > 0) {

		/* Add a data-in aka write aka program
		 * instruction. Give each of these instructions a
		 * pointer into the buffer parm to indicate the data
		 * to write.  Use cursor to indicate the start of this
		 * portion of the buffer parm.
		 */
		size_this_page = (size_remaining < available ?
			size_remaining : available);
		operation.instrs[i].type = NAND_OP_DATA_IN_INSTR;
		operation.instrs[i].ctx.data_in.len = size_this_page;
		operation.instrs[i].ctx.data_in.buf = &buffer[cursor];
		i++;
		
		/* Add a C_PROGRAM_EXECUTE command. */
		operation.instrs[i].type = NAND_OP_CMD_INSTR;
		operation.instrs[i].ctx.cmd.opcode = C_PROGRAM_EXECUTE;
		i++;
		
		/* Add a waitrdy instruction. */
		operation.instrs[i].type = NAND_OP_WAITRDY_INSTR;
		operation.instrs[i].ctx.waitrdy.timeout_ms =
			TIMEOUT_WRITE_PAGE_US;
		i++;
		
		size_remaining -= size_this_page;
		cursor += size_this_page;
		assert(cursor + size_remaining == size);
		available = PAGE_SIZE;      /* Subsequent page capacity. */
		
	} /* while bytes remain to transfer */

	operation.ninstrs = i;  /* record how many instructions we added */
	
	assert(operation.ninstrs == instruction_count_data_xfer(BYTE(offset),
		size));

#ifdef DIAGNOSTICS
	print_operation(&operation);
#endif
	
	if (driver.operation.exec_op(&operation)) {
		ret_val = -1;  /* timeout */
	}

	free(operation.instrs);
	return ret_val;

} /* exec_write() */


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

	struct nand_operation operation; /* the NAND operation to send */
	unsigned int i = 0;              /* counts instructions in operation */
	unsigned int cursor = 0;         /* index into buffer parm */
	unsigned int size_remaining;     /* bytes left to transfer */
	unsigned int available;          /* space available in current page */
	unsigned int size_this_page;     /* bytes xferred in current page */
	int ret_val = 0;                 /* optimistically presume success */

#ifdef DIAGNOSTICS
	printf("Framework exec_read() start addr 0x%08x "
		"size 0x%08x instruction count 0x%08x.\n",
	       offset, size, instruction_count_data_xfer(BYTE(offset), size));
#endif 
	
	operation.instrs = malloc(instruction_count_data_xfer(BYTE(offset),
		size) * sizeof(struct nand_op_instr));
	assert(operation.instrs != NULL);

	/* Begin with a C_READ_SETUP. */
	operation.instrs[i].type = NAND_OP_CMD_INSTR;
	operation.instrs[i].ctx.cmd.opcode = C_READ_SETUP;
	i++;

	/* Then specify the address. */
	operation.instrs[i].type = NAND_OP_ADDR_INSTR;
	operation.instrs[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_IO;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] = BLOCK(offset);
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_PAGE ]  = PAGE(offset);
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BYTE ]  = BYTE(offset);
	i++;

	/* The driver expects to transfer the data one page at a time.
	 * The first page is special: the data must begin at
	 * BYTE(offset), and if BYTE(offset) is not 0 (that is, if the
	 * beginning of the transfer is not page-aligned) the first
	 * page won't be able to hold a full PAGE_SIZE of bytes.  All
	 * subsequent pages will be able to hold up to PAGE_SIZE
	 * bytes.  Use 'available' to manage this first
	 * page/subsequent pages capacity logic. Add a transfer,
	 * execute, waitrdy trio for each page to the operation.
	 */
	size_remaining = size;
	available = PAGE_SIZE - BYTE(offset);  /* First page capacity. */
	while (size_remaining > 0) {

		/* Add a C_READ_EXECUTE command. */
		operation.instrs[i].type = NAND_OP_CMD_INSTR;
		operation.instrs[i].ctx.cmd.opcode = C_READ_EXECUTE;
		i++;
		
		/* Add a waitrdy instruction. */
		operation.instrs[i].type = NAND_OP_WAITRDY_INSTR;
		operation.instrs[i].ctx.waitrdy.timeout_ms =
			TIMEOUT_READ_PAGE_US;
		i++;

		/* Add a data-out aka read instruction. Give each of
		 * these instructions a pointer into the buffer parm
		 * to indicate where to put the data.  Use cursor to
		 * indicate the start of this portion of the buffer
		 * parm.
		 */
		size_this_page = (size_remaining < available ?
			size_remaining : available);
		operation.instrs[i].type = NAND_OP_DATA_OUT_INSTR;
		operation.instrs[i].ctx.data_in.len = size_this_page;
		operation.instrs[i].ctx.data_in.buf = &buffer[cursor];
		i++;
		
		size_remaining -= size_this_page;
		cursor += size_this_page;
		assert(cursor + size_remaining == size);
		available = PAGE_SIZE;      /* Subsequent page capacity. */
		
	} /* while bytes remain to transfer */

	operation.ninstrs = i;  /* record how many instructions we added */
	
	assert(operation.ninstrs == instruction_count_data_xfer(BYTE(offset),
		size));

#ifdef DIAGNOSTICS
	print_operation(&operation);
#endif
	
	if (driver.operation.exec_op(&operation)) {
		ret_val = -1;  /* timeout */
	}

	free(operation.instrs);
	return ret_val;
	
} /* exec_read() */


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
	
	struct nand_operation operation; /* the NAND operation to send */
	unsigned char start_block;  /* block number of first block to erase */
	unsigned int num_blocks;    /* number of complete blocks to erase */
	int i = 0;                  /* counts instructions */
	unsigned int b;             /* counts blocks */
	int ret_val = 0;            /* optimistically presume success */
	
	/* The offset and size input parms describe the region to
	 * erase in terms of bytes.  Describe it in terms of blocks,
	 * instead.  Note that we erase complete blocks, so we'll have
	 * to enlarge the region to erase if offset isn't the start of
	 * a block or if size isn't a multiple of the block size.
	 */
	start_block = offset / BLOCK_SIZE;
	size += offset % BLOCK_SIZE;  /* part of block ahead of region start */
	num_blocks  = size / BLOCK_SIZE;
	if (size % BLOCK_SIZE) num_blocks++;  /* Round up for partial blocks */

#ifdef DIAGNOSTICS
	printf("Framework exec_erase() start block 0x%02x "
		"num blocks 0x%02x instruction count 0x%08x.\n",
		start_block, num_blocks, instruction_count_erase(num_blocks));
#endif 

	operation.instrs = malloc(instruction_count_erase(num_blocks)
		* sizeof(struct nand_op_instr));
	assert(operation.instrs != NULL);
	
	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION

	operation.instrs[i].type = NAND_OP_CMD_INSTR;
	operation.instrs[i++].ctx.cmd.opcode = C_ERASE_SETUP;

	operation.instrs[i].type = NAND_OP_ADDR_INSTR;
	operation.instrs[i].ctx.addr.naddrs = NAND_INSTR_NUM_ADDR_ERASE;
	operation.instrs[i].ctx.addr.addrs[ NAND_INSTR_BLOCK ] =
		start_block;
	i++;

	for (b = 0; b < num_blocks; b++) {
		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_CMD_INSTR;
		operation.instrs[i++].ctx.cmd.opcode = C_ERASE_EXECUTE;

		operation.ninstrs++;
		operation.instrs[i].type = NAND_OP_WAITRDY_INSTR;
		operation.instrs[i++].ctx.waitrdy.timeout_ms =
			TIMEOUT_ERASE_BLOCK_US;
	}

	assert(operation.ninstrs == instruction_count_erase(num_blocks));

#ifdef DIAGNOSTICS
	print_operation(&operation);
#endif
	
	if (driver.operation.exec_op(&operation))
		ret_val = -1;  /* timeout */

	free(operation.instrs);
	return ret_val;
}
