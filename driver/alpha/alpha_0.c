/* Copyright (c) 2022 Provatek, LLC.
 * Copyright (c) 2023 Timothy Jon Fraser Consulting LLC.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "clock.h"
#include "framework.h"
#include "device_emu.h"
#include "driver.h"


volatile unsigned long* driver_ioregister;

// Resets the nand device to its inital state
void nand_set_register(unsigned char offset, unsigned char value)
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
void nand_read(unsigned char *buffer, unsigned int length)
{
	while (length--) {
		*buffer++ = *((unsigned char*)driver_ioregister + IOREG_DATA);
	}
}

// Writes the data in buffer to the nand device at offset with length of size
void nand_program(unsigned char *buffer, unsigned int length)
{
	while (length--) {
		*((unsigned char*)driver_ioregister + IOREG_DATA) = 
			*buffer++;
	}
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
	printf("ALPHA 0 DRIVER\n");
	driver_ioregister = ioregister;
	return old_dib;  /* This driver does not use DIB. */
}
