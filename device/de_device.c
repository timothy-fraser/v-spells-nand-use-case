// Copyright (c) 2022 Provatek, LLC.

/*
 * device_emu.c - This is the device emulator.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "device_emu.h"
#include "framework.h"

#include "de_gpio.h"
#include "de_deadline.h"
#include "de_device.h"
#include "de_cursor.h"
#include "de_ioregs.h"

/* IO Register Masks */
#define MASK_COMMAND 0x00FF0000
#define MASK_ADDRESS 0x0000FF00
#define MASK_DATA    0x000000FF

/* bits to shift IO register to obtain value */
#define COMMAND_SHIFT 16
#define ADDRESS_SHIFT 8


/* array to store the simulated flash storage */
unsigned char data_store[NUM_BLOCKS * NUM_PAGES * NUM_BYTES];

/* array to store the cache */
unsigned char cache[NUM_BYTES];

extern volatile unsigned long *ioregs; /* from de_ioregs.c */
extern unsigned int cursor;            /* from de_cursor.c */
extern unsigned long deadline;         /* from de_deadline.c */

/* machine state */
unsigned int machine_state = MS_INITIAL_STATE;

/*
 * clear_state()
 *
 * in:  cursor - the current value of the cursor
 *      deadline - the current value of the deadline state variable
 *      cache - the current contents of the cache
 * out: cursor - the cursor reset to zero
 *      deadline - the deadline reset to zero
 *      cache - the contents of the cache reset to zero
 * return: nothing
 *
 * Clears the internal device emulator state, including the cursor, deadline,
 * and cache.
 */

void
clear_state(void) {
	cursor = 0;
	deadline = 0; 
	memset(cache, 0, sizeof(cache));
}




/*
 * handle_watchpoint_ioregisters()
 *
 * in:  child_pid - PID of the child tracee
 *      p_regs    - pointer to register struct containing tracee's
 *                  register values
 * out: p_regs    - registers may be updated to change value read from
 *                  ioregisters
 *      machine_state - may be updated based on IO register inputs
 *      cursor        - may be updated based on IO register inputs
 *      cache         - may be updated based on IO register inputs
 *      data_store    - may be updated based on IO register inputs
 *      deadline      - may be set based on IO register inputs
 * return: nothing
 *
 * This function handles tracee reads and writes to the ioregisters
 * variable.  It implements a state machine that determines how to
 * handle the values in the ioregister variable. See the manual for
 * details on this state machine.
 */

static
void handle_watchpoint_ioregisters(pid_t child_pid,
	struct user_regs_struct *p_regs) {

	unsigned int peeked;
	unsigned int command;
	unsigned char cache_byte;
	unsigned int return_value;
				  
	peeked = ptrace(PTRACE_PEEKDATA, child_pid, ioregs, NULL);
	command = (peeked & MASK_COMMAND) >> COMMAND_SHIFT;

	switch (machine_state) {
	case MS_INITIAL_STATE:
		switch (command) {
		case C_READ_SETUP:
			machine_state = MS_READ_AWAITING_BLOCK_ADDRESS;
			break;
		case C_PROGRAM_SETUP:
			machine_state = MS_PROGRAM_AWAITING_BLOCK_ADDRESS;
			break;
		case C_ERASE_SETUP:
			machine_state = MS_ERASE_AWAITING_BLOCK_ADDRESS;
			break;
		default:
			machine_state = MS_BUG;
			break;
		}
		break;

	case MS_READ_AWAITING_BLOCK_ADDRESS:
		if (before_deadline() || (command != C_READ_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS)
				>> ADDRESS_SHIFT, CURSOR_BLOCK_SHIFT);
			machine_state = MS_READ_AWAITING_PAGE_ADDRESS;
		}
		break;

	case MS_READ_AWAITING_PAGE_ADDRESS:
		if (before_deadline() || (command != C_READ_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS)
				>> ADDRESS_SHIFT, CURSOR_PAGE_SHIFT);
			machine_state = MS_READ_AWAITING_BYTE_ADDRESS;
		}
		break;

	case MS_READ_AWAITING_BYTE_ADDRESS:
		if (before_deadline() || (command != C_READ_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS)
				>> ADDRESS_SHIFT, CURSOR_BYTE_SHIFT);
			machine_state = MS_READ_AWAITING_EXECUTE;
		}
		break;

	case MS_READ_AWAITING_EXECUTE:
		if (before_deadline() || (command != C_READ_EXECUTE)) {
			machine_state = MS_BUG;
		} else {
			set_deadline(READ_PAGE_DURATION);

			memcpy(cache,
			       &data_store[cursor & ~CURSOR_BYTE_MASK],
			       NUM_BYTES);

			machine_state = MS_READ_PROVIDING_DATA;

			ptrace(PTRACE_POKEDATA,
			       child_pid,
			       ioregs,
			       C_DUMMY << COMMAND_SHIFT);
		}
		break;

	case MS_READ_PROVIDING_DATA:
		if (before_deadline()) {
			machine_state = MS_BUG;
		} else {
			switch (command) {
			case C_DUMMY:
				cache_byte = 
					cache[cursor & CURSOR_BYTE_MASK];
				return_value = (C_DUMMY << COMMAND_SHIFT)
					| cache_byte;
				/* Make the child tracee believe it has read
				 * return_value from its data register.
				 */
				ptrace(PTRACE_POKEDATA,
				       child_pid,
				       ioregs,
				       return_value);
				update_tracee_cpu_registers(child_pid, p_regs,
					return_value);
				
				increment_cursor(false);
				break;
			case C_READ_EXECUTE:
				set_deadline(READ_PAGE_DURATION);

				memcpy(cache,
				       &data_store[cursor & ~CURSOR_BYTE_MASK],
				       NUM_BYTES);

				machine_state = MS_READ_PROVIDING_DATA;

				ptrace(PTRACE_POKEDATA,
				       child_pid,
				       ioregs,
				       C_DUMMY << COMMAND_SHIFT);
				break;
			case C_READ_SETUP:
				clear_state();
				machine_state = MS_READ_AWAITING_BLOCK_ADDRESS;
				break;
			case C_PROGRAM_SETUP:
				clear_state();
				machine_state =
					MS_PROGRAM_AWAITING_BLOCK_ADDRESS;
				break;
			case C_ERASE_SETUP:
				clear_state();
				machine_state = MS_ERASE_AWAITING_BLOCK_ADDRESS;
				break;
			default:
				machine_state = MS_BUG;
				break;
			}
		}
		break;

	case MS_PROGRAM_AWAITING_BLOCK_ADDRESS:
		if (before_deadline() || (command != C_PROGRAM_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS)
				>> ADDRESS_SHIFT, CURSOR_BLOCK_SHIFT);
			machine_state = MS_PROGRAM_AWAITING_PAGE_ADDRESS;
		}
		break;

	case MS_PROGRAM_AWAITING_PAGE_ADDRESS:
		if (before_deadline() || (command != C_PROGRAM_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS) >>ADDRESS_SHIFT,
					CURSOR_PAGE_SHIFT);
			machine_state = MS_PROGRAM_AWAITING_BYTE_ADDRESS;
		}
		break;

	case MS_PROGRAM_AWAITING_BYTE_ADDRESS:
		if (before_deadline() || (command != C_PROGRAM_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS)
				>> ADDRESS_SHIFT, CURSOR_BYTE_SHIFT);
			machine_state = MS_PROGRAM_ACCEPTING_DATA;

			ptrace(PTRACE_POKEDATA,
			       child_pid,
			       ioregs,
			       C_DUMMY << COMMAND_SHIFT);
		}
		break;

	case MS_PROGRAM_ACCEPTING_DATA:
		if (before_deadline()) {
			machine_state = MS_BUG;
		} else {
			switch (command) {
			case C_DUMMY:
				cache[cursor & CURSOR_BYTE_MASK] =
					peeked & MASK_DATA;
				increment_cursor(true);
				break;
			case C_PROGRAM_EXECUTE:
				set_deadline(WRITE_PAGE_DURATION);

				for (int i=0; i < NUM_BYTES; i++) {
					int idx = (cursor & ~CURSOR_BYTE_MASK);
					idx += i;
					data_store[idx] = cache[i];
				}

				memset(cache, 0, sizeof(cache));
				increment_page();

				ptrace(PTRACE_POKEDATA,
				       child_pid,
				       ioregs,
				       C_DUMMY << COMMAND_SHIFT);

				break;
			case C_READ_SETUP:
				clear_state();
				machine_state = MS_READ_AWAITING_BLOCK_ADDRESS;
				break;
			case C_PROGRAM_SETUP:
                                clear_state();
                                machine_state =
					MS_PROGRAM_AWAITING_BLOCK_ADDRESS;
                                break;
			case C_ERASE_SETUP:
				clear_state();
				machine_state =
					MS_ERASE_AWAITING_BLOCK_ADDRESS;
				break;
			default:
				machine_state = MS_BUG;
				break;
			}
		}
		break;

	case MS_ERASE_AWAITING_BLOCK_ADDRESS:
		if (before_deadline() || (command != C_ERASE_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS)
				>> ADDRESS_SHIFT, CURSOR_BLOCK_SHIFT);
			machine_state = MS_ERASE_AWAITING_EXECUTE;
		}
		break;

	case MS_ERASE_AWAITING_EXECUTE:
		if (before_deadline()) {
			machine_state = MS_BUG;
		} else {
			switch (command) {
			case C_ERASE_EXECUTE:
				set_deadline(ERASE_BLOCK_DURATION);

				/* Erase ignores page and byte offset parts
				 * of the cursor and erases a whole block.
				 */
				memset(&data_store[(cursor &
					CURSOR_BLOCK_MASK)],
					0,
					NUM_PAGES*NUM_BYTES);

				ptrace(PTRACE_POKEDATA,
				       child_pid,
				       ioregs,
				       C_DUMMY << COMMAND_SHIFT);

				increment_block();
				break;
			case C_READ_SETUP:
				clear_state();
				machine_state = MS_READ_AWAITING_BLOCK_ADDRESS;
				break;
			case C_PROGRAM_SETUP:
				clear_state();
				machine_state =
					MS_PROGRAM_AWAITING_BLOCK_ADDRESS;
				break;
			case C_ERASE_SETUP:
				clear_state();
				machine_state =
					MS_ERASE_AWAITING_BLOCK_ADDRESS;
				break;
			default:
				machine_state = MS_BUG;
				break;
			}
		}
		break;

	case MS_BUG:
		printf("device emulator: in machine state bug.\n");
		exit(1);
		break;

	default:
		machine_state = MS_BUG;
		printf("device emulator: in machine state bug.\n");
		exit(1);
		break;
	}
}

/*
 * device_init()
 *
 * in:     in_ioregisters - a pointer to the ioregisters variable from main.c
 *         child_pid - PID of child tracee
 * out:    none
 * return: none
 *
 * Initialize debugging support for the child process reads and writes to the
 * ioregisters variable. Also initialize breakpoint support for the gpio_set()
 * and gpio_get() functions. Then begin waiting for child process activity.
 */

void
device_init(volatile unsigned long *in_ioregisters, pid_t child_pid) {
	
	int child_status;       /* child process status returned by wait() */
	struct user_regs_struct regs; /* hold tracee register values. */

	ioregs = in_ioregisters;

	/* Wait for first trap. */
	wait(&child_status);

	memset(data_store, 0, sizeof(data_store));
	clear_state();

	/* Set up hardware watchpoint on child tracee's ioregisters variable
	 * so that the child tracee will trap and pass control to the parent
	 * tracer whenever the child tracee reads or writes ioregisters.
	 */
	handle_watchpoint_setup(child_pid);

	/* Setup complete.  Let tracee continue. */
	ptrace(PTRACE_CONT, child_pid, NULL, NULL);

	/* Parent tracer loops handling traps until the child tracee exits. */
	while (1)
	{
		/* Wait until the tracee either hits a watchpoint or
		 * terminates.
		 */
		wait(&child_status);

		/* The tracee will trap under four conditions:
		 *  (1) tracee reached the end of its program and terminated,
		 *  (2) tracee hit its gpio_set() breakpoint,
		 *  (3) tracee hit its gpio_get() breakpoint, or
		 *  (4) tracee hit is ioregisters watchpoint.
		 * Use our knowledge of the tracee's breakpoint and
		 * watchpoint configuration to figure out which
		 * condition actually happened and handle it.
		 */

		if (WIFEXITED(child_status))
			break; /* child done, we're done. */

		/* Get tracee's registers to help us figure out what
		 * function it was running when it trapped.
		 */
		ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);

		/* Figure out which breakpoint or watchpoint the
		 * tracee hit and handle it.
		 */
		if (RIP_IN_GPIO_SET(regs.rip)) {
			handle_breakpoint_gpio_set(&regs);
		} else if (RIP_IN_GPIO_GET(regs.rip)) {
			handle_breakpoint_gpio_get(child_pid, &regs);
		} else {
			handle_watchpoint_ioregisters(child_pid, &regs);
                }

		/* Let the tracee continue. */
		ptrace(PTRACE_CONT, child_pid, NULL, NULL);
	}
}
