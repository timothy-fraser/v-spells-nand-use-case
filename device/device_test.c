// Copyright (c) 2022 Provatek, LLC.

/*
 * device_test.c - This file implements unit tests for the device emulator.
 *                 It re-implements portions of the ptrace tracer/tracee
 *                 functionality in order to test the use of the ioregisters
 *                 and the gpio functions.
 */

#include <assert.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/user.h>
#include <time.h>

#include "device_emu.h"
#include "framework.h"

/* IO Register Masks */
#define MASK_COMMAND 0x00FF0000
#define MASK_ADDRESS 0x0000FF00
#define MASK_DATA    0x000000FF

#define BUSY_SLEEP_DURATION 1000 /* microseconds */

#define COMMAND_SHIFT 16

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

}

/* This word in memory represents our emulated IO registers.
 * When we fork(), we will get a new child process that is a (nearly)
 * identical copy of the parent.  Consequently, both the parent and the
 * child will have this variable, and both of them will have it at the
 * same address in their address spaces.  The parent will use it to
 * determine the address of the ioregister word in the child process and
 * to remember the value it most recently PEEKed from the child.
 * The child will read and write from its copy.
 *
 * It is important that this variable be an unsigned long.  The parent
 * tracer will use ptrace(POKE_DATA) to modify the value of this
 * variable in the tracee's memory.  ptrace(POKE_DATA) modifies an
 * entire unsigned long; if this variable were smaller the update
 * might overwrite part of a nearby variable.
 */
volatile unsigned long ioregisters = 0;

/* wait_for_device()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Checks the status pin of the device using the gpio_get() function
 * and returns once the device is ready.
 */
void wait_for_device()
{
	while (gpio_get(PN_STATUS) != DEVICE_READY) {
		usleep(BUSY_SLEEP_DURATION);
	}
}

/*
 * The following are the function prototypes for test cases for
 * the device emulator
 */
void wr_page_test(void);
void erase_block_test(void);
void wr_less_than_page_test(void);
void write_two_pages_test(void);
void wr_two_pages_test(void);
void write_last_two_pages_test(void);
void erase_two_blocks_test(void);
void erase_last_two_blocks_test(void);
void overwrite_page_test(void);
void write_and_read_two_pages_test(void);
void reset_test(void);
void set_status_pin_test(void);
void get_reset_pin_test(void);
void write_and_read_all_pages_test(void);
void erase_all_blocks_test(void);

/*
 * tester_main()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * This is the "main" routine for the child tracee.  It calls the
 * test cases for the device emulator.
 */
int tester_main(void)
{
	pid_t child_pid;          /* my pid, the child tracee */

	/* Initiate a trace.  Parent is the tracer, child is the tracee. */
	ptrace(PTRACE_TRACEME, 0, NULL, NULL);
	child_pid = getpid();

	/* Child pauses itself so that parent can set up watchpoints. */
	kill(child_pid, SIGTRAP);

	/* Test cases */
	wr_page_test();
	erase_block_test();
	wr_less_than_page_test();
	write_two_pages_test();
	write_last_two_pages_test();
	erase_two_blocks_test();
	erase_last_two_blocks_test();
	overwrite_page_test();
	write_and_read_two_pages_test();
	reset_test();
	set_status_pin_test();
	get_reset_pin_test();
	//write_and_read_all_pages_test(); // takes ~1 hour to run this test
	erase_all_blocks_test();

	return 0;
}

/*
 * wr_page_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Writes an entire page to device storage and reads it back in.
 * Asserts if the read bytes are not 0xFF.
 */
void wr_page_test()
{
	unsigned char data[256];
	memset(data,0xFF,256);

	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}
	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0xFF) &&
		       "expected (ioregisters & MASK_DATA) == 0xFF");
	}
}

/* erase_block_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Writes an entire page to device storage and then erases the block that
 * contains that page. Asserts if the read bytes are not 0x00.
 */
void erase_block_test()
{
	wr_page_test();

	ioregisters = C_ERASE_SETUP << COMMAND_SHIFT;
	ioregisters = C_ERASE_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_ERASE_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}
}

/*
 * wr_less_than_page_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Write less than an entire page at offset in the page.
 * Asserts if the bytes before and after the written bites are not 0x00.
 * Asserts if the written bytes are not 0xFF.
 */
void wr_less_than_page_test()
{
	unsigned char data[10];
	memset(data,0xFF,10);

	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000A00; /* byte address */

	for (int i=0;i<10;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}
	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	// read first 10 bytes, should by 0x00
	for (int i=0;i<10;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}

	// read next 10 bytes, should by 0xFF
	for (int i=10;i<20;i++) {
		assert(((ioregisters & MASK_DATA) == 0xFF) &&
		       "expected (ioregisters & MASK_DATA) == 0xFF");
	}

	// read next 10 bytes, should by 0x00
	for (int i=20;i<30;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}
}

/*
 * write_two_pages_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Writes two consecutive pages with a single program setup command
 * and reads them back with two separate read commands.
 * Asserts if the first page bytes are not 0xAA and the second page
 * bytes are not 0xBB.
 */
void write_two_pages_test()
{
	unsigned char data[256];
	memset(data,0xAA,256);

	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	memset(data,0xBB,256);
	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// read first page
	unsigned long data_reg;
	for (int i=0;i<256;i++) {
		data_reg = ioregisters & MASK_DATA;
		assert((data_reg == 0xAA) &&
		       "expected (ioregisters & MASK_DATA) == 0xFF");
	}

	// read second page
	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000100; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	for (int i=0;i<256;i++) {
		data_reg = ioregisters & MASK_DATA;
		assert((data_reg == 0xBB) &&
		       "expected (ioregisters & MASK_DATA) == 0xFF");
	}
}

/*
 * write_last_two_pages_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Writes two consecutive pages with a single program setup command
 * starting at the last page in storage and wrapping back to the first
 * page. It then reads them back with two separate read commands.
 * Asserts if the last page bytes are not 0xAA and the first page
 * bytes are not 0xBB.
 */
void write_last_two_pages_test()
{
	unsigned char data[256];
	memset(data,0xAA,256);

	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x0000FF00; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	memset(data,0xBB,256);
	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x0000FF00; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// read first page
	unsigned long data_reg;
	for (int i=0;i<256;i++) {
		data_reg = ioregisters & MASK_DATA;
		assert((data_reg == 0xAA) &&
		       "expected (ioregisters & MASK_DATA) == 0xFF");
	}

	// read second page
	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	for (int i=0;i<256;i++) {
		data_reg = ioregisters & MASK_DATA;
		assert((data_reg == 0xBB) &&
		       "expected (ioregisters & MASK_DATA) == 0xFF");
	}
}

/*
 * write_and_read_two_pages_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Writes two consecutive pages with a single program setup command
 * and reads them back with a single read setup command.
 * Asserts if the first page bytes are not 0xAA and the second page
 * bytes are not 0xBB.
 */
void write_and_read_two_pages_test()
{
	erase_block_test();

	unsigned char data[256];
	memset(data,0xAA,256);

	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	memset(data,0xBB,256);
	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// read first page
	unsigned long data_reg;
	for (int i=0;i<256;i++) {
		data_reg = ioregisters & MASK_DATA;
		assert((data_reg == 0xAA) && "expected (ioregisters & MASK_DATA) == 0xAA");
	}

	// read second page
	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	for (int i=0;i<256;i++) {
		data_reg = ioregisters & MASK_DATA;
		assert((data_reg == 0xBB) && "expected (ioregisters & MASK_DATA) == 0xBB");
	}
}

/*
 * erase_two_blocks_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Writes to pages in two consecutive blocks and then erases them with one erase
 * setup command. Asserts if either pages bytes are not 0x00 after the erase.
 */
void erase_two_blocks_test()
{
	unsigned char data[256];
	memset(data,0xAA,256);

	// write first page in first block
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// write first page in second block
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000100; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	memset(data,0xBB,256);
	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// confirm the writes worked
	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0xAA) &&
		       "expected (ioregisters & MASK_DATA) == 0xAA");
	}

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000100; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0xBB) &&
		       "expected (ioregisters & MASK_DATA) == 0xBB");
	}

	// Erase both blocks
	ioregisters = C_ERASE_SETUP << COMMAND_SHIFT;
	ioregisters = C_ERASE_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */

	// first block erase
	ioregisters = C_ERASE_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// second block erase
	ioregisters = C_ERASE_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// Confirm the erases worked
	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000100; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}
}

/*
 * erase_last_two_blocks_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Writes to pages in two consecutive blocks and then erases them with one erase
 * setup command starting at the last block and wrapping back around to the
 * first block. Asserts if either pages bytes are not 0x00 after the erase.
 */
void erase_last_two_blocks_test()
{
	unsigned char data[256];
	memset(data,0xAA,256);

	// write first page in last block
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// write first page in first block
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	memset(data,0xBB,256);
	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// confirm the writes worked
	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0xAA) &&
		       "expected (ioregisters & MASK_DATA) == 0xAA");
	}

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0xBB) &&
		       "expected (ioregisters & MASK_DATA) == 0xBB");
	}

	// Erase both blocks
	ioregisters = C_ERASE_SETUP << COMMAND_SHIFT;
	ioregisters = C_ERASE_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */

	// first block erase
	ioregisters = C_ERASE_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// second block erase
	ioregisters = C_ERASE_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// Confirm the erases worked
	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}
}

/*
 * overwrite_page_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Write more than an entire page to the same page and read it back in.
 * First writes 0xAA to the first 128 bytes of a page.  Then writes 0xDD for 256 bytes,
 * causing the device emulator cache to wrap back around on itself.
 * Asserts if the page bytes are not all 0xDD after the overwrite.
 */
void overwrite_page_test()
{
	unsigned char data[256];
	memset(data,0xAA,256);

	// Clear any existing data
	erase_block_test();

	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	// write the first 128 bytes
	for (int i=0;i<128;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	// write 256 bytes, starting at the 129th byte, so the first 128 bytes will be
	// overwritten when the cursor wraps to stay in the page
	memset(data,0xDD,256);
	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	unsigned long data_reg = ioregisters & MASK_DATA;
	for (int i=0;i<256;i++) {
		data_reg = ioregisters & MASK_DATA;
		assert((data_reg == 0xDD) &&
		       "expected (ioregisters & MASK_DATA) == 0xDD");
	}
}

/*
 * reset_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Write an entire page to device cache, but don't store it. Set the GPIO reset pin instead.
 * Then actually write an entire page, read it back in, and then erase its block to confirm
 * the device emulator is still functioning.
 */
void reset_test()
{
	unsigned char data[256];
	memset(data,0xCC,256);

	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	gpio_set(PN_RESET, true);

	erase_block_test(); 
}

/*
 * set_status_pin_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Calls the gpio_set() function to set the status pin, which is a meaningless operation.
 * It then writes an entire page to the device, reads it back in, and erases its block
 * to confirm the device emulator is still functioning.
 */
void set_status_pin_test()
{
	gpio_set(PN_STATUS,1);
	erase_block_test(); 
}

/*
 * get_reset_pin_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Calls the gpio_get() function on the reset pin, which is a meaningless operation.
 * Confirms that the device emulator returns 0.
 */
void get_reset_pin_test()
{
	int ret = gpio_get(PN_RESET);
	assert((ret == 0) && "expected ret == 0");
}

/*
 * write_and_read_all_pages_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * NOTE: this test takes about an hour to run.
 * Writes all consecutive pages with a single program setup command
 * and reads all of them back with a single read setup command.
 * Asserts if all page bytes are not 0xEE.
 */
void write_and_read_all_pages_test()
{
	unsigned char data[256];
	memset(data,0xEE,256);

	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) { // blocks
		for (int j=0;j<256;j++) { // pages
			for (int k=0;k<256;k++) { // bytes
				ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[k];
			}

			ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
			wait_for_device();
		}

		printf("Wrote block %d\n",i);
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	unsigned long data_reg;
	for (int i=0;i<256;i++) { // blocks
		for (int j=0;j<256;j++) { // pages
			ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;
			wait_for_device();

			for (int k=0;k<256;k++) { // bytes
				data_reg = ioregisters & MASK_DATA;
				assert((data_reg == 0xEE) &&
				       "expected (ioregisters & MASK_DATA) == 0xEE");
			}
		}

		printf("Checked block %d\n",i);
	}
}

/*
 * erase_all_blocks_test()
 *
 * in:     none
 * out:    none
 * return: none
 *
 * Writes to pages in the first and last blocks and then erases all blocks with
 * one erase setup command. Asserts if either pages bytes are not 0x00 after the erase.
 */
void erase_all_blocks_test()
{
	unsigned char data[256];
	memset(data,0xAA,256);

	// write first page in last block
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// write first page in first block
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT;
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_PROGRAM_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	memset(data,0xBB,256);
	for (int i=0;i<256;i++) {
		ioregisters = (C_DUMMY << COMMAND_SHIFT) | data[i];
	}

	ioregisters = C_PROGRAM_EXECUTE << COMMAND_SHIFT;
	wait_for_device();

	// confirm the writes worked
	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0xAA) &&
		       "expected (ioregisters & MASK_DATA) == 0xAA");
	}

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0xBB) &&
		       "expected (ioregisters & MASK_DATA) == 0xBB");
	}

	// Erase all blocks
	ioregisters = C_ERASE_SETUP << COMMAND_SHIFT;
	ioregisters = C_ERASE_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */

	for (int i=0;i<256;i++) {
		ioregisters = C_ERASE_EXECUTE << COMMAND_SHIFT;
		wait_for_device();
	}

	// Confirm the erases worked
	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x0000FF00; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}

	ioregisters = C_READ_SETUP << COMMAND_SHIFT;
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* block address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* page address */
	ioregisters = C_READ_SETUP << COMMAND_SHIFT | 0x00000000; /* byte address */

	ioregisters = C_READ_EXECUTE << COMMAND_SHIFT;

	wait_for_device();

	for (int i=0;i<256;i++) {
		assert(((ioregisters & MASK_DATA) == 0x00) &&
		       "expected (ioregisters & MASK_DATA) == 0x00");
	}
}

int main()
{
	pid_t child_pid;          /* receives what fork() gives us. */

	switch (child_pid = fork()) {

	case -1: /* fork() failed. */
		perror("Failed to fork");
		exit(-1);

	case 0:  /* I am the child. */
		tester_main();
		break;

	default: /* I am the parent; child_pid holds child pid. */
		device_init(&ioregisters, child_pid);
	}

	/* Both child and parent end up here. */
	exit(0);
}
