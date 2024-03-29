/* Copyright (c) 2022 Provatek, LLC.
 * Copyright (c) 2023 Timothy Jon Fraser Consulting LLC.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "clock.h"
#include "framework.h"
#include "device_emu.h"
#include "driver.h"

#define NAND_CONTROLLER_CHIP_COUNT 1
#define NAND_STORAGE_CHIPS_PER_CONTROLLER 1
#define NAND_DEVICE_COUNT 1
#define MAX_NAND_DEVICES 64
#define MAX_STORAGE_CHIPS 8


int exec_op(struct nand_operation *commands);

volatile unsigned long* driver_ioregister;

struct nand_storage_chip kilo_storage_chip = {
	.nblocks = NUM_BLOCKS,
	.npages_per_block = NUM_PAGES,
	.nbytes_per_page = NUM_BYTES,
	.ref_count = 1,
	.next_storage = NULL,
	.controller = NULL    /* set this during initialization */
};

struct nand_controller_chip kilo_controller_chip = {
	.exec_op = exec_op,
	.nstorage = NAND_STORAGE_CHIPS_PER_CONTROLLER,
	.ref_count = NAND_STORAGE_CHIPS_PER_CONTROLLER,
	.first_storage = &kilo_storage_chip,
	.last_storage = &kilo_storage_chip
};

struct nand_device kilo_device = {
	.next_device = NULL,   /* set this during initialization */
	.ref_count = NAND_CONTROLLER_CHIP_COUNT + 1,
	.controller = &kilo_controller_chip,
	.device_makemodel = "Provatek, LLC NAND Provastore"
};

// Resets the nand device to its inital state
void nand_set_register(unsigned int offset, unsigned char value)
{
	*((unsigned char*)driver_ioregister + offset) = value;
}

// Waits for device status to be ready for an action
int nand_wait(unsigned int interval_us)
{
	/* We're trying to mimic what real Linux device drivers see: a
	 * volatile jiffies variable whose value increases
	 * monotonically with clock ticks.  The now() function has
	 * similar behavior.
	 */
	timeus_t timeout = now() + interval_us;

	do {
		if (gpio_get(PN_STATUS) == DEVICE_READY) {
			return 0;
		}
		usleep(NAND_POLL_INTERVAL_US);
	} while(now() < timeout);

	return ((gpio_get(PN_STATUS) == DEVICE_READY) ? 0 : -1);
}

// Reads the data in to buffer in the nand device at offset with length of size
void nand_read(unsigned char* buffer, unsigned int length)
{

	for (unsigned int i = 0; i < length; i++) {
		buffer[i] = *((unsigned char*)driver_ioregister + IOREG_DATA);
	}

}

// Writes the data in buffer to the nand device at offset with length of size
void nand_program(const unsigned char* buffer, unsigned int length)
{

	for (unsigned int i = 0; i < length; i++) {
		*((unsigned char*)driver_ioregister + IOREG_DATA) = buffer[i];
	}

}

// Performs functionality simular to exec_op in linux kernal
// Returns 0 on success
int exec_op(struct nand_operation *commands)
{
	struct nand_op_instr command;
	unsigned int addr_len;
	for (int i = 0; i < commands->ninstrs; i++) {
		command = commands->instrs[i];
		switch (command.type)
		{
		case NAND_OP_CMD_INSTR:
			nand_set_register(IOREG_COMMAND, 
				command.ctx.cmd.opcode);
			break;
		case NAND_OP_ADDR_INSTR:
			addr_len = command.ctx.addr.naddrs;
			for(int j = 0; j < addr_len; j++) {
				nand_set_register(IOREG_ADDRESS, 
					command.ctx.addr.addrs[j]);
			}
			break;
		case NAND_OP_DATA_IN_INSTR:
			nand_program(command.ctx.data_in.buf, 
				command.ctx.data_in.len);
			break;
		case NAND_OP_DATA_OUT_INSTR:
			nand_read(command.ctx.data_out.buf,
				command.ctx.data_out.len);
			break;
		case NAND_OP_WAITRDY_INSTR:
			if (nand_wait(command.ctx.waitrdy.timeout_ms))
				return -1;  /* timeout */
			break;
		default:
			printf("Unknown exec_op data.\n");
			return -1;
		}
	}
	return 0;
}

/* register_nand_device()
 *
 * in:     old_dib - pointer to the DIB as it exists before this
 *                   driver's initialization.
 * return: condition value
 *         --------- -----
 *         error     NULL
 *         success   pointer to a new DIB that has this driver's
 *                   device as its first device, followed by
 *                   whatever was in the old DIB.
 * 
 * If we fing the DIB in a well-formed state, add a an entry to the
 * DIB describing this device.  Otherwise return NULL.
 */

struct nand_device *register_nand_device(struct nand_device *old_dib)
{
	/* Refuse to interact with a malformed initial DIB. */
	if (verify_dib(old_dib)) return NULL;

	/* Link our device into a new DIB. */
	kilo_storage_chip.controller = &kilo_controller_chip;
	/* kilo_device.next_device = old_dib; */
	kilo_device.next_device = NULL;  /* BUG: loses old DIB entry */
	return &kilo_device;  /* our device is the first in the new DIB */
}

// Initalizes the private device information
struct nand_device *init_nand_driver(volatile unsigned long *ioregister,
	struct nand_device *old_dib)
{
	printf("KILO 1 DRIVER\n");
	driver_ioregister = ioregister;
	return register_nand_device(old_dib);
}

struct nand_driver get_driver()
{
	struct nand_driver ret = {
		.type = NAND_EXEC_OP,
		.operation.exec_op = kilo_device.controller->exec_op,
	};
	return ret;
}
