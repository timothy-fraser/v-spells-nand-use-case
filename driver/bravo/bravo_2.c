// Copyright (c) 2022 Provatek, LLC.

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "framework.h"
#include "driver.h"
#include "device_emu.h"

#define NAND_POLL_INTERVAL_US 10  /* polling interval in microseconds */

volatile unsigned long* driver_ioregister;

static unsigned char curCmd = 0;
// Resets the nand device to its inital state
void nand_set_register(unsigned char offset, unsigned char value)
{
	if (offset == IOREG_COMMAND)
		curCmd = value;
	*((unsigned char*)driver_ioregister + offset) = value;
}

// Waits for device status to be ready for an action
// Intended BUG: timeout ignored
int nand_wait(unsigned int interval_us)
{
	/* Some explanation on this timeout computation:
	 *
	 * We're trying to mimic what real Linux device drivers see: a
	 * volatile jiffies variable whose value increases
	 * monotonically with clock ticks.  The clock() function has
	 * similar behavior.  My Linux's bits/time.h indicates that
	 * clock_t ticks are always microseconds, so rather than
	 * converting microseconds to ticks using CLOCKS_PER_SEC /
	 * 1000000, I'm just adding.  Although I worry that I'm losing
	 * POSIX points by doing so, the simple addition mimics the
	 * pattern real Linux device drivers would use.
	 */
	/* clock_t timeout = clock() + interval_us;  BUG */
	unsigned int status;

	while (DEVICE_READY != (status = gpio_get(PN_STATUS))) {
		if (curCmd == C_ERASE_EXECUTE) {
			while (status != DEVICE_READY) {
				status = gpio_get(PN_STATUS);
				usleep(NAND_POLL_INTERVAL_US);
			}
		}
	}
	return 0;
}

// Reads the data in to buffer in the nand device at offset with length of size
// Returns number of bytes read
int nand_read(unsigned char *buffer, unsigned int length)
{
	unsigned int page_size = NUM_BYTES;
	if (length > page_size) {
		return -1;
	}

	for (unsigned int i = 0; i < length; i++) {
		buffer[i] = *((unsigned char*)driver_ioregister + IOREG_DATA);
	}

	return length;
}

// Writes the data in buffer to the nand device at offset with length of size
// Returns number of bytes wrote
int nand_program(unsigned char *buffer, unsigned int length)
{
	unsigned int page_size = NUM_BYTES;
	if (length > page_size) {
		return -1;
	}

	for (unsigned int i = 0; i < length; i++) {
		*((unsigned char*)driver_ioregister + IOREG_DATA) = buffer[i];
	}

	return length;
}

struct nand_driver get_driver()
{
	struct nand_jump_table njt = {
		.read_buffer = nand_read,
		.set_register = nand_set_register,
		.wait_ready = nand_wait,
		.write_buffer = nand_program
	};

	struct nand_driver nd = {
		.type = NAND_JUMP_TABLE,
		.operation.jump_table = njt,
	};
	return nd;
}

// Initalizes the private device information
struct nand_device *init_nand_driver(volatile unsigned long *ioregister,
	struct nand_device *old_dib)
{
	printf("BRAVO 2 DRIVER\n");
	driver_ioregister = ioregister;
	return old_dib;  /* This driver does not use DIB. */
}
