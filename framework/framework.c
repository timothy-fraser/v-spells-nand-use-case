// Copyright (c) 2022 Provatek, LLC.

#include <memory.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "framework.h"
#include "driver.h"
#include "device_emu.h"

/* This is an x86/amd64-specific assembly breakpoint instruction that
 * causes the process that executes it to trap to its debugger.  It
 * uses GCC-specific asm() syntax.  Programs typically do not include
 * this breakpoint instruction in their source; tracers (that is,
 * debuggers) typically insert breakpoints into tracees by modifying
 * their program text, writing this breakpoint instruction to the
 * location where they want the tracee to trap and remembering the
 * original instruction the new breakpoint instruction overwrote so
 * they can restore it and let the tracee continue after they've
 * handled the breakpoint.  Restoring the original instruction clears
 * the breakpoint so the tracee won't trap a second time.  We never
 * want to clear our breakpoint; we always want the tracee to trap, so
 * rather than have the tracer modify the tracee's program text we'll
 * simply make the breakpoint instruction part of the tracee's
 * original program text.
 */

#define BREAKPOINT asm("int $3")

/* gpio_set()
 *
 * in:     pin   - the pin number to set.
 *         value - the value to set, either 0 or non-0.
 * out:    none
 * return: none
 *
 * Traps to the parent tracer, expecting the parent tracer to emulate
 * the following functionality:
 *   If value is 0, clears the GPIO pin indicated by pin.
 *   Otherwise, sets the GPIO pin indicagted by pin.
 */

void gpio_set(unsigned int pin, unsigned int value)
{
	BREAKPOINT;
}

/* gpio_get()
 *
 * in:     pin   - the pin number to get.
 * out:    none
 * return: the value of the pin'th pin, either 0 or 1.
 *
 * Traps to the parent tracer, expecting the parent tracer to emulate
 * the following functionality:
 *   Returns 0 if the pin'th pin is clear, otherwise returns 1.
 */

unsigned int gpio_get(unsigned int pin)
{
	/* It is important that this local variable be an unsigned long.
	 * The tracer will modify the value of this variable using
	 * ptrace(PTRACE_POKEDATA).  The ptrace() function modifies data in
	 * unsigned-long-sized chunks.  If this variable was smaller than an
	 * unsigned long, the tracer's unsigned-long-sized write would
	 * overwrite part of whatever variable was above this one on the
	 * stack.
	 */
	unsigned long retval = 0x00; /* tracee may modify this value */
	BREAKPOINT;
	return retval;

} /* gpio_get() */

static struct nand_driver driver;

struct nand_device *init_framework(volatile unsigned long *ioregister,
	struct nand_device *old_dib)
{
	struct nand_device *new_dib;  /* DIB possibly updated by driver */
	
	new_dib = init_nand_driver(ioregister, old_dib);
	driver = get_driver();

	/* Initiate a trace.  Parent is the tracer, child is the tracee. */
	ptrace(0, 0, NULL, NULL);

	/* Child pauses itself so that parent can set up watchpoints. */
	kill(getpid(), 5);

	return new_dib;
}


/* jt_write()
 *
 * in:     buffer - array of bytes to write to storage device
 *         offset - device address to receive data
 *         size   - number of bytes to write, can be multiple pages
 * out:    nothing
 * return: -1 on error (specifically, device timeout) else number
 *         of bytes written to the device.
 *          
 * Writes size bytes from buffer to NAND storage device.  Writes them
 * to storage starting at the storage address in offset.
 *
 * This version of write works with drivers that provide the framework
 * with a jump table of functions rather than a command interpreter.
 *
 */

int jt_write(unsigned char *buffer, unsigned int offset, unsigned int size) {
	
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
		if (offset != 0) {
			size_to_write = bytes_per_page - offset;
			offset = 0;
		}
		if (bytes_left < bytes_per_page &&
			bytes_left < size_to_write) {
			size_to_write = bytes_left;
		}

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
	return size;
}


/* exec_write()
 *
 * in:     buffer - array of bytes to write to storage device
 *         offset - device address to receive data
 *         size   - number of bytes to write, can be multiple pages
 * out:    nothing
 * return: -1 on error (specifically, device timeout) else number
 *         of bytes written to the device.
 *          
 * Writes size bytes from buffer to NAND storage device.  Writes them
 * to storage starting at the storage address in offset.
 *
 * This version of write works with drivers that provide the framework
 * with a command interpreter rather than a jump table.
 *
 */

int exec_write(const unsigned char* buffer, unsigned int offset,
	unsigned int size) {
	
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

	struct nand_operation operation;
	struct nand_op_instr instructions[4096];
	
	int i = 0;
	operation.ninstrs = 0;
	operation.instrs = instructions;

	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION

	instructions[i].type = NAND_OP_CMD_INSTR;
	instructions[i++].ctx.cmd.opcode = C_PROGRAM_SETUP;

	instructions[i].type = NAND_OP_ADDR_INSTR;
	const unsigned char start_addr[] = {block_addr, page_addr, byte_addr};
	instructions[i].ctx.addr.naddrs = 3;
	instructions[i++].ctx.addr.addrs = start_addr;

	do {
		size_to_write = bytes_per_page;
		if (offset != 0) {
			size_to_write = bytes_per_page - offset;
			offset = 0;
		}
		if (bytes_left < bytes_per_page &&
			bytes_left < size_to_write) {
			size_to_write = bytes_left;
		}
		operation.ninstrs++;
		instructions[i].type = NAND_OP_DATA_IN_INSTR;
		instructions[i].ctx.data.len = size_to_write;
		instructions[i++].ctx.data.buf.in = &buffer[cursor];

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
		return -1;  /* timeout or exceeded page */

	return size;
}

int write_nand(unsigned char *buffer, unsigned int offset, unsigned int size)
{
	if (driver.type == NAND_JUMP_TABLE)
	{
		return jt_write(buffer, offset, size);
	}
	else if (driver.type == NAND_EXEC_OP)
	{
		return exec_write(buffer, offset, size);
	}
	return -1;
}

int jt_read(unsigned char *buffer, unsigned int offset, unsigned int size)
{
	unsigned int bytes_per_page = NUM_BYTES;
	unsigned int pages_per_block = NUM_PAGES;
	unsigned int blocks_per_chip = NUM_BLOCKS;

	unsigned int bytes_left = size;
	unsigned int cursor = 0;

	unsigned char byte_addr = offset % bytes_per_page;
	unsigned char page_addr = offset / pages_per_block;
	unsigned char block_addr = offset / (pages_per_block * blocks_per_chip);

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
		if (offset != 0) {
			size_to_read = bytes_per_page - offset;
			offset = 0;
		}
		if (bytes_left < bytes_per_page && bytes_left < size_to_read) {
			size_to_read = bytes_left;
		}

		driver.operation.jump_table.read_buffer(&buffer[cursor], 
			size_to_read);

		cursor += size_to_read;
		bytes_left -= size_to_read;
	}
	return size;
}

int exec_read(unsigned char* buffer, unsigned int offset, unsigned int size)
{
	unsigned int bytes_per_page = NUM_BYTES;
	unsigned int pages_per_block = NUM_PAGES;
	unsigned int blocks_per_chip = NUM_BLOCKS;

	unsigned int bytes_left = size;
	unsigned int cursor = 0;

	unsigned char byte_addr = offset % bytes_per_page;
	unsigned char page_addr = offset / pages_per_block;
	unsigned char block_addr = offset / (pages_per_block * blocks_per_chip);

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
	const unsigned char start_addr[] = {block_addr, page_addr, byte_addr};
	instructions[i].ctx.addr.naddrs = 3;
	instructions[i++].ctx.addr.addrs = start_addr;

	do {
		operation.ninstrs++;
		instructions[i].type = NAND_OP_CMD_INSTR;
		instructions[i++].ctx.cmd.opcode = C_READ_EXECUTE;

		operation.ninstrs++;
		instructions[i].type = NAND_OP_WAITRDY_INSTR;
		instructions[i++].ctx.waitrdy.timeout_ms =
			TIMEOUT_READ_PAGE_US;
	
		size_to_read = bytes_per_page;
		if (offset != 0) {
			size_to_read = bytes_per_page - offset;
			offset = 0;
		}
		if (bytes_left < bytes_per_page && bytes_left < size_to_read) {
			size_to_read = bytes_left;
		}

		operation.ninstrs++;
		instructions[i].type = NAND_OP_DATA_OUT_INSTR;
		instructions[i].ctx.data.len = size_to_read;
		instructions[i++].ctx.data.buf.out = &buffer[cursor];

		cursor += size_to_read;
		bytes_left -= size_to_read;

	} while(bytes_left > 0);

	if (driver.operation.exec_op(&operation))
		return -1;  /* timeout or exceeded page */

	return size;
}

int read_nand(unsigned char *buffer, unsigned int offset, unsigned int size)
{
	if (driver.type == NAND_JUMP_TABLE)
	{
		return jt_read(buffer, offset, size);
	}
	else if (driver.type == NAND_EXEC_OP)
	{
		return exec_read(buffer, offset, size);
	}

	return -1;
}

int jt_erase(unsigned int offset, unsigned int size)
{
	unsigned int pages_per_block = NUM_PAGES;
	unsigned int blocks_per_chip = NUM_BLOCKS;

	unsigned char start_block_addr = offset / 
		(pages_per_block * blocks_per_chip);
	unsigned char end_block_addr = (offset + size) / 
		(pages_per_block * blocks_per_chip);

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
	return size;
}

int exec_erase(unsigned int offset, unsigned int size)
{
	unsigned int pages_per_block = NUM_PAGES;
	unsigned int blocks_per_chip = NUM_BLOCKS;

	unsigned char start_block_addr = offset / 
		(pages_per_block * blocks_per_chip);
	unsigned char end_block_addr = (offset + size) / 
		(pages_per_block * blocks_per_chip);

	struct nand_operation operation;
	struct nand_op_instr instructions[4096];
	
	int i = 0;
	operation.ninstrs = 0;
	operation.instrs = instructions;

	operation.ninstrs = 2; // SETUP + ADDR INSTRUCTION

	instructions[i].type = NAND_OP_CMD_INSTR;
	instructions[i++].ctx.cmd.opcode = C_ERASE_SETUP;

	instructions[i].type = NAND_OP_ADDR_INSTR;
	const unsigned char start_addr[] = {start_block_addr};
	instructions[i].ctx.addr.naddrs = 1;
	instructions[i++].ctx.addr.addrs = start_addr;

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

	return size;
}


int erase_nand(unsigned int offset, unsigned int size)
{
	if (driver.type == NAND_JUMP_TABLE)
	{
		return jt_erase(offset, size);
	}
	else if (driver.type == NAND_EXEC_OP)
	{
		return exec_erase(offset, size);
	}

	return -1;
}

/* verify_storage()
 *
 * in:     p_storage - the DIB storage node to verify
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     DIB storage node is well-formed
 *          -1     DIB storage node is not well-formed
 *
 * Examines DIB storage node and indicates whether or not it is well-formed.
 *
 */

static int verify_storage(struct nand_controller_chip *p_controller,
	struct nand_storage_chip *p_storage) {

	/* The first and last storage chips in the list point back to
	 * the controller.  The rest have NULLs in their controller
	 * field.
	 */
	if ((p_controller->first_storage == p_storage) ||
	    (p_controller->last_storage == p_storage)) {
		if (p_storage->controller != p_controller) {
			puts("DIB: first and last storage chip do not "
				"point to controller chip.");
			return -1;
		}
	} else {
		if (p_storage->controller != NULL) {
			puts("DIB: middle controller chips have "
				"non-NULL controller fields.");
			return -1;
		}
	}

	/* Ref count field must be 1. */
	if (p_storage->ref_count != 1) {
		puts("DIB: storage chip has reference count != 1.");
		return -1;
	}

	/* Well-formed storage node! */
	return 0;

} /* verify_storage() */


/* verify_controller()
 *
 * in:     p_controller - the DIB controller to verify
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     DIB controller is well-formed
 *          -1     DIB controller is not well-formed
 *
 * Examines DIB controller and indicates whether or not it is well-formed.
 *
 */

static int verify_controller(struct nand_controller_chip *p_controller) {

	unsigned int sc_count = 0;            /* count of controller chips */
	struct nand_storage_chip *p_sc;     /* iterates thru storage chips */
	struct nand_storage_chip *p_last;     /* last storage chip in list */

	/* Controller chip must have at least one storage chip. */
	if (!p_controller->first_storage) {
		puts("DIB:  controller chip has no storage chips.");
		return -1;
	}
	
	/* Examine each of the controller's storage chips. */
	for (p_sc = p_controller->first_storage; p_sc != NULL;
		p_sc = p_sc->next_storage) {

		/* Controller can have no more than max number of
		 * storage chips.
		 */
		if (++sc_count > MAX_STORAGE_CHIPS) {
			puts("DIB:  controller chip has too many "
				"storage chips.");
			return -1;
		}

		/* Each storage chip must be well-formed. */
		if (verify_storage(p_controller, p_sc)) return -1;

		/* remember last storage chip we've seen. */
		p_last = p_sc;
	}
	
	/* The controller must be linked to the last storage chip in
	 * the list.
	 */
	if (p_controller->last_storage != p_last) {
		puts("DIB: controller chip not linked to last "
			"storage chip.");
		return -1;
	}

	/* The controller's reference count must equal its count of
	 * storage nodes, and its storage node count must be accurate.
	 */
	if ((p_controller->ref_count != p_controller->nstorage) ||
	    (p_controller->nstorage != sc_count)) {
		puts("DIB: controller reference count is incorrect.");
		return -1;
	}
	
	/* Well-formed controller node! */
	return 0;
	
} /* verify_controller() */


/* verify_device()
 *
 * in:     p_device - the DIB device to verify
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     DIB device is well-formed
 *          -1     DIB device is not well-formed
 *
 * Examines DIB device and indicates whether or not it is well-formed.
 *
 */

static int verify_device(struct nand_device *p_device) {

	/* Each device must have a controller. */
	if (!p_device->controller) {
		puts("DIB: device has no controller chip.");
		return -1;
	}

	/* The controller must be well-formed. */
	if (verify_controller(p_device->controller)) return -1;
	
	/* Device's reference count must be one greater than its
	 * controller's reference count.
	 */
	if (p_device->ref_count != p_device->controller->ref_count + 1) {
		puts("DIB: device reference count is incorrect.");
		return -1;
	}

	/* Well-formed device! */
	return 0;
	
} /* verify_device() */


/* verify_dib()
 *
 * in:     dib - the DIB to verify
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     DIB is well-formed
 *          -1     DIB is not well-formed
 *
 * Examines DIB and indicates whether or not it is well-formed.
 *
 */

int verify_dib(struct nand_device *dib) {

	unsigned int device_count = 0; /* count of devices in DIB */
	struct nand_device *p_device;  /* iterates through DIB devices */

	/* Verify each device in the DIB.  Note that a NULL DIB is an
	 * empty DIB, and this is still considered well-formed.
	 */
	for(p_device = dib; p_device != NULL;
		p_device = p_device->next_device) {

		printf("Verifying: %s.\n", p_device->device_makemodel);

		/* DIB must have no more than max number of devices. */
		if (++device_count > MAX_NAND_DEVICES) return -1;

		if (verify_device(p_device)) return -1;  /* malformed DIB */
	}
	
	return 0;  /* well-formed DIB */

} /* verify_dib() */
