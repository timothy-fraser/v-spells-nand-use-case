/*
 * Device emulator input message parser module.
 *
 * Copyright (c) 2022 Provatek, LLC.
 * Copyright (c) 2023 Timothy Jon Fraser Consulting LLC.
 *
 */

#include <sys/types.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h> 

#include "device_emu.h"
#include "de_deadline.h"
#include "de_store.h"
#include "de_ioregs.h"


/* Device Emulator states */
#define MS_INITIAL_STATE 0x00000000
#define MS_BUG           0x00000001

/* Device Emulator READ states */
#define MS_READ_AWAITING_BLOCK_ADDRESS 0x00000002
#define MS_READ_AWAITING_PAGE_ADDRESS  0x00000003
#define MS_READ_AWAITING_BYTE_ADDRESS  0x00000004
#define MS_READ_AWAITING_EXECUTE       0x00000005
#define MS_READ_PROVIDING_DATA         0x00000006

/* Device Emulator PROGRAM states */
#define MS_PROGRAM_AWAITING_BLOCK_ADDRESS 0x00000007
#define MS_PROGRAM_AWAITING_PAGE_ADDRESS  0x00000008
#define MS_PROGRAM_AWAITING_BYTE_ADDRESS  0x00000009
#define MS_PROGRAM_ACCEPTING_DATA         0x0000000A

/* Device Emulator ERASE states */
#define MS_ERASE_AWAITING_BLOCK_ADDRESS 0x0000000B
#define MS_ERASE_AWAITING_EXECUTE       0x0000000C


static volatile unsigned long *ioregs; /* address of ioregisters variable */
static unsigned int machine_state;     /* parser finite state machine state */


/* clear_state()
 *
 * in:     nothing
 * out:    cursor - the cursor reset to zero
 *         deadline - the deadline reset to zero
 *         cache - the contents of the cache reset to zero
 * return: nothing
 *
 * Clears the internal device emulator state, including the cursor, deadline,
 * and cache.
 */

static void
clear_state(void) {

	deadline_clear(); 
	store_clear_cursor();
	store_clear_cache();
	
} /* clear_state() */


/* parser_reset()
 *
 * in:     nothing
 * out:    machine_state updated, plus clear_state() side effects
 * return: nothing
 *
 * Clears internal device emulator state and sets machine_state to
 * MS_INITIAL_STATE.  This is the function to call on GPIO reset pin
 * set.
 *
 */

void
parser_reset(void) {
	clear_state();
	machine_state = MS_INITIAL_STATE;
} /* parser_reset() */


/* parser_init()
 *
 * in:     in_ioregisters - address of ioregisters variable
 * out:    ioregs set to in_ioregisters
 *         machine_state set to MS_INITIAL_STATE
 *         deadline_init() side effects
 *         store_init() side effects
 *
 * Initializes the parser and its deadline and store sub-modules.
 * This is the function to call on startup; it produces a blank
 * storage device ready to handle its first request.
 *
 */

void
parser_init(volatile unsigned long *in_ioregisters) {
	
	ioregs = in_ioregisters;
	machine_state = MS_INITIAL_STATE;
	deadline_init();
	store_init();
	
} /* parser_init() */


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

void
handle_watchpoint_ioregisters(pid_t child_pid,
	struct user_regs_struct *p_regs) {

	unsigned int peeked;
	unsigned int command;
	unsigned char cache_byte;
	unsigned int return_value;
				  
	peeked = ptrace(PTRACE_PEEKDATA, child_pid, ioregs, NULL);
	command = (peeked & MASK_COMMAND) >> COMMAND_SHIFT;

#ifdef DIAGNOSTICS	
	printf("Device emulator in state %02u received "
	       "C=%02u, A=0x%02x, D=0x%02x.\n",
	       machine_state, command,
	       (peeked & MASK_ADDRESS) >> ADDRESS_SHIFT,
	       (peeked & MASK_DATA));
#endif
	
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
			store_copy_page_to_cache();
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
				cache_byte = store_get_cache_byte();
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
				store_copy_page_to_cache();
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
				store_set_cache_byte(peeked & MASK_DATA);
				increment_cursor(true);
				break;
			case C_PROGRAM_EXECUTE:
				set_deadline(WRITE_PAGE_DURATION);
				store_copy_page_from_cache();
				store_clear_cache();
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
				store_erase_block();

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
