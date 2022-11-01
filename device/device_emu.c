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

/* IO Register Masks */
#define MASK_COMMAND 0x00FF0000
#define MASK_ADDRESS 0x0000FF00
#define MASK_DATA    0x000000FF

/* cursor address masks */
#define CURSOR_BLOCK_MASK 0x00FF0000
#define CURSOR_PAGE_MASK  0x0000FF00
#define CURSOR_BYTE_MASK  0x000000FF

/* cursor bit shifts */
#define CURSOR_BLOCK_SHIFT 16
#define CURSOR_PAGE_SHIFT   8
#define CURSOR_BYTE_SHIFT   0

/* bits to shift IO register to obtain value */
#define COMMAND_SHIFT 16
#define ADDRESS_SHIFT 8

/* Durations (microseconds) */
#define READ_PAGE_DURATION   100
#define WRITE_PAGE_DURATION  600
#define ERASE_BLOCK_DURATION 2000
#define RESET_DURATION       500

#define MICROSECONDS_IN_SECOND 1000000

/* IO registers pointer, base variable defined in main.c */
volatile unsigned long *ioregs;

/* array to store the simulated flash storage */
unsigned char data_store[NUM_BLOCKS * NUM_PAGES * NUM_BYTES];

/* array to store the cache */
unsigned char cache[NUM_BYTES];

/* cursor */
unsigned int cursor = 0;

/* deadline (in microseconds since epoch) */
unsigned long deadline = 0;

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
static void clear_state(void)
{
	cursor = 0;
	deadline = 0; 
	memset(cache, 0, sizeof(cache));
}

/*
 * before_deadline()
 *
 * in:  deadline - the current value of the deadline state variable
 * out: nothing
 * return: true - if the current system time is earlier than the deadline.
 *         false - if the current system time is the same or later than the
 *		   deadline.
 *
 * Checks to see if the current system time is before the deadline state
 * variable or not.
 */
static int before_deadline()
{
	struct timeval cur_time;

	gettimeofday(&cur_time, NULL);
	unsigned long cur_total = (cur_time.tv_sec * MICROSECONDS_IN_SECOND) +
				  cur_time.tv_usec;

	if (cur_total < deadline)
		return true;

	return false;
}

/*
 * set_deadline()
 *
 * in:  deadline - the current value of the deadline state variable
 *      duration - the duration (in microseconds) to add to the current time
 * out: deadline - set to the current time plus the duration
 * return: nothing
 *
 * Sets the deadline state variable to be the current system time plus the
 * specified duration.
 */
static void set_deadline(int duration)
{
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	deadline = (cur_time.tv_sec * MICROSECONDS_IN_SECOND) +
		   cur_time.tv_usec + duration;
}

/*
 * increment_cursor()
 *
 * in:  cursor - the current value of the cursor
 *      remain - wrap cursor to remain in current page or not
 * out: cursor - the cursor incremented by one
 * return: nothing
 *
 * Increments the cursor by one. If remain is true, then the cursor is wrapped
 * to stay within the same page. Otherwise the cursor is only wrapped back to
 * zero when it is incremented past the end of storage.
 */
static void increment_cursor(bool remain)
{
	if (remain) {
		unsigned int bytes = cursor & CURSOR_BYTE_MASK;
		if (bytes + 1 < NUM_BYTES)
			cursor += 1;
		else
			cursor = cursor & ~CURSOR_BYTE_MASK;
			
	} else {
		cursor += 1;

		// wrap cursor to remain in storage
		if (cursor >= (NUM_BLOCKS * NUM_PAGES * NUM_BYTES))
			cursor = 0;
	}
}

/*
 * increment_page()
 *
 * in:  cursor - the current value of the cursor
 * out: cursor - the cursor incremented to the beginning of the next page
 * return: nothing
 *
 * Increments the cursor to the next page. Increments the block to the next
 * block if the current page is the last page in a block. Wraps the cursor
 * back to zero if this is the last page in storage.
 */
static void increment_page()
{
	unsigned int page = cursor & CURSOR_PAGE_MASK;
	page = page >> CURSOR_PAGE_SHIFT;

	unsigned int block = cursor & CURSOR_BLOCK_MASK;
	block = block >> CURSOR_BLOCK_SHIFT;

        page += 1;

        if (page >= NUM_PAGES) {
		page = 0;
		block += 1;
		if (block >= NUM_BLOCKS)
			block = 0;
	}

        cursor = 0;
	cursor = cursor | (block << CURSOR_BLOCK_SHIFT);
	cursor = cursor | (page << CURSOR_PAGE_SHIFT);
}

/*
 * increment_block()
 *
 * in:  cursor - the current value of the cursor
 * out: cursor - the cursor incremented to the beginning of the next block
 * return: nothing
 *
 * Increments the cursor to the next block. Wraps the cursor back to zero if
 * this is the last block in storage.
 */
static void increment_block()
{
	unsigned int block = cursor & CURSOR_BLOCK_MASK;
	block = block >> CURSOR_BLOCK_SHIFT;

	block += 1;

	if (block >= NUM_BLOCKS) {
		block = 0;
	}

	cursor = 0;
	cursor = cursor | (block << CURSOR_BLOCK_SHIFT);
}

/*
 * set_cursor_byte()
 *
 * in:  cursor - the current value of the cursor
 *      value - the value to set the cursor byte to
 *      shift - the byte in the cursor to set (left shift)
 * out: cursor - the cursor with the specified byte set
 * return: nothing
 *
 * Sets the specified byte in the cursor to value.
 */
static void set_cursor_byte(unsigned int value, unsigned int shift)
{       
	cursor = cursor & ~(0xFF << shift); // clear the byte
	cursor = cursor | (value << shift);
}

/*
 * handle_breakpoint_gpio_set()
 *
 * in:  p_regs - pointer to register struct containing tracee's register values
 * out: machine_state - reset to the initial state if reset pin is set 
 * return: nothing
 *
 * This function processes tracee calls to its gpio_set() function.
 */
static void handle_breakpoint_gpio_set(struct user_regs_struct *p_regs)
{
	/* The following code depends on some details specific to the amd64
	 * GCC ABI: the instruction pointer is in rip, the first argument to
	 * a called function is in rdi, and the second argument is in rsi.
	*/
	switch (p_regs->rdi) {
	case PN_STATUS:
		break;
	case PN_RESET:
		if (p_regs->rsi == true) {
			clear_state();
			machine_state = MS_INITIAL_STATE;
			usleep(RESET_DURATION);
		}
		break;
	}
}

/* handle_breakpoint_gpio_get()
 *
 * in:  child_pid - PID of the child tracee
 *      p_regs - pointer to register struct containing tracee's register values
 * out: nothing
 * return: nothing
 *
 * This function processes tracee calls to its gpio_get() function.
 */
static void handle_breakpoint_gpio_get(pid_t child_pid,
				struct user_regs_struct *p_regs)
{
	long long unsigned int rva; /* addr of tracee gpio_get()
				       retval local var */

	/* We want to use POKE to reset the value of the tracee's gpio_get()
	 * retval local variable.  retval lives RETVAL_OFFSET bytes
	 * from the start of the stack frame indicated by the tracee's rbp
	 * register.
	 */
	rva = p_regs->rbp - sizeof(unsigned long);

	switch (p_regs->rdi) {
	case PN_STATUS:
		if (before_deadline()) {
			ptrace(PTRACE_POKEDATA, child_pid, rva, DEVICE_BUSY);
		} else {
			ptrace(PTRACE_POKEDATA, child_pid, rva, DEVICE_READY);
		}
		break;
	case PN_RESET:
		ptrace(PTRACE_POKEDATA, child_pid, rva, 0);
		break;
	}
}

/* handle_watchpoint_setup()
 *
 * in:     child_pid - the PID of the child tracee
 * out:    none
 * return: none
 *
 * Sets up read-write watchpoint on the tracee's ioregisters variable.
 *
 */
static void handle_watchpoint_setup(pid_t child_pid)
{
        /* This sequence of PTRACE_POKEUSER operations sets one of the four
         * AMD64 hardware debug registers to be a read-write watchpoint on
         * the ioregister variable.  I figured out this sequence by running
         * strace gdb on a similar tracee program and manually asking gdb to
         *   set can-use-hw-watchpoints
         *   awatch ioregisters
         * and looking to see how gdb used ptrace() in the strace log.
         */
        /* Put the address of the var we want to watch in debug reg 0. */
        ptrace(PTRACE_POKEUSER, child_pid, offsetof(struct user, u_debugreg),
        ioregs);
        /* Set the watch for read-write. */
        ptrace(PTRACE_POKEUSER, child_pid, offsetof(struct user, u_debugreg) +
               56, 0xF0101);
        /* Not sure what this is for.  Mask? */
        ptrace(PTRACE_POKEUSER, child_pid, offsetof(struct user, u_debugreg) +
               48, 0);
}


/* update_tracee_cpu_registers()
 *
 * in:      child_pid - process ID of tracee child process.
 *          p_regs    - child tracee CPU register values at watchpoint
 *                      activation.
 *          value     - value we want the child tracee to read from the
 *                      emulated storage device's ioregisters.
 * out:     p_regs    - child tracee CPU register values updates to
 *                      reflect read from emulated ioregisters.
 *
 * This function enables the parent tracer device emulator to modify
 * the state of the child tracee process's CPU registers to emulate a
 * read from the ioregisters variable.
 *
 * When the child tracee reads from the emulated storage device's
 * ioregisters variable, it uses a mov instruction that triggers a
 * watchpoint.  That watchpoint pauses the child tracee and activates
 * the parent tracer.  When the parent tracer sees the watchpoint
 * activation, it can update the contents of the ioregisters variable
 * in the child tracee's memory to reflect the value the child tracee
 * ought to have read.  However, this memory update comes too late -
 * the child tracee has already moved whatever value was previously in
 * the ioregisters variable in memory into one of its CPU registers.
 * This function enables the parent tracer to figure out which CPU
 * register the child tracee used and update its value, as well.
 */

/* Macro to get the nth byte from unsigned long ul. */
#define BYTE(ul,n) (((ul) >> ((n)*8)) & 0xFFUL)

static void update_tracee_cpu_registers(pid_t child_pid,
	struct user_regs_struct *p_regs,
	unsigned int value) {

	unsigned long instructions;

	/* After watchpoint activation, the traccee's instruction
	 * pointer "rip" points to the instruction *after* the
	 * instruction that triggered the watchpoint.  We want to
	 * examine the instruction that triggered the watchpoint, but
	 * we're not sure how long it is.  We'll peek an unsigned
	 * long's worth of bytes from the tracee's program text that
	 * preceeds where rip points.
	 */
	instructions = ptrace(PTRACE_PEEKDATA, child_pid,
		(p_regs->rip - sizeof(unsigned long)), NULL);

	/* The watchpoint was triggered by some kind of mov
	 * instruction.  There are many many flavors of mov.  In our
	 * drivers, we are usually picking a single byte out of the
	 * unsigned long ioregister variable.  GCC seems to like using
	 * the one-byte flavor of the movzx "move with zero extend"
	 * instruction for this purpose.  It's usual pattern is to
	 * load the address of the ioregisters variable into a
	 * register and then use the movzx instruction to move the
	 * one-byte value at that address to the same register,
	 * triggering the watchpoint like so:
	 *
	 *       0f b6 00                movzbl (%rax),%eax  
	 *
	 * Its choice of register is unpredictable.  We've seen RAX
	 * and RDX.  RBX and RCX seem possible.  It's also possible
	 * that GCC will use an entirely different form of mov.
	 *
	 * Examine the instructions and, at least for the forms of mov
	 * we understand, figure out which CPU register has the value
	 * the mov read, and update that register.
	 */

	if ((BYTE(instructions, 5) == 0x0f) &&
	    (BYTE(instructions, 6) == 0xb6)) {

		/* This is the one-byte flavor of the movzx "move with
		 * zero extend". It's a three-byte instruction with
		 * the third byte indicating the source and
		 * destination of the move in ModRM format.  See
		 * tables 2-1 and 2-2 from the Intel 64 and IA-32
		 * architecture software developer's manual.
		 */
		switch (BYTE(instructions, 7)) {
		case 0x00:
			/* AX to AX */
			p_regs->rax = value;
			break;
		case 0x1B:
			/* BX to BX */
			p_regs->rbx = value;
			break;
		case 0x09:
			/* CX to CX */
			p_regs->rcx = value;
			break;
		case 0x12:
			/* DX to DX */
			p_regs->rdx = value;
			break;
		default:
			/* unhanded form of arguments */
			goto error;
			
		}

		/* Set the tracee's registers to our updated values. */
		ptrace(PTRACE_SETREGS, child_pid, NULL, p_regs);
		return;
		
	} /* if/else handled instruction types */

error:
	/* If we wind up here, we saw an instruction we don't know how
	 * to handle.  Report unhandled instruction text and halt. */
	printf("The driver has used a machine instruction that the device\n"
	       "emulator does not yet understand.  Please include the\n"
	       "following program text bytes in a bug report:\n");
	printf("Program text: %02lx %02lx %02lx %02lx %02lx %02lx\n",
	       BYTE(instructions, 2), BYTE(instructions, 3),
	       BYTE(instructions, 4), BYTE(instructions, 5),
	       BYTE(instructions, 6), BYTE(instructions, 7));
	exit(-1);

} /* update_tracee_cpu_registers() */


/*
 * handle_watchpoint_ioregisters()
 *
 * in:  child_pid - PID of the child tracee
 *      p_regs - pointer to register struct containing tracee's register values
 * out: p_regs - registers may be updated to change value read from ioregisters
 *      machine_state - may be updated based on IO register inputs
 *      cursor - may be updated based on IO register inputs
 *      cache - may be updated based on IO register inputs
 *      data_store - may be updated based on IO register inputs
 *      deadline - may be set based on IO register inputs
 * return: nothing
 *
 * This function handles tracee reads and writes to the ioregisters variable.
 * It implements a state machine that determines how to handle the values in
 * the ioregister variable. See the manual for details on this state machine.
 */
static void handle_watchpoint_ioregisters(pid_t child_pid,
				   struct user_regs_struct *p_regs)
{
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
			set_cursor_byte((peeked & MASK_ADDRESS) >>ADDRESS_SHIFT,
					CURSOR_BLOCK_SHIFT);
			machine_state = MS_READ_AWAITING_PAGE_ADDRESS;
		}
		break;

	case MS_READ_AWAITING_PAGE_ADDRESS:
		if (before_deadline() || (command != C_READ_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS) >>ADDRESS_SHIFT,
					CURSOR_PAGE_SHIFT);
			machine_state = MS_READ_AWAITING_BYTE_ADDRESS;
		}
		break;

	case MS_READ_AWAITING_BYTE_ADDRESS:
		if (before_deadline() || (command != C_READ_SETUP)) {
			machine_state = MS_BUG;
		} else {
			set_cursor_byte((peeked & MASK_ADDRESS) >>ADDRESS_SHIFT,
					CURSOR_BYTE_SHIFT);
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
				return_value =
					(C_DUMMY << COMMAND_SHIFT) | cache_byte;
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
			set_cursor_byte((peeked & MASK_ADDRESS) >>ADDRESS_SHIFT,
					CURSOR_BLOCK_SHIFT);
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
			set_cursor_byte((peeked & MASK_ADDRESS) >>ADDRESS_SHIFT,
					CURSOR_BYTE_SHIFT);
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
				machine_state = MS_ERASE_AWAITING_BLOCK_ADDRESS;
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
			set_cursor_byte((peeked & MASK_ADDRESS) >>ADDRESS_SHIFT,
					CURSOR_BLOCK_SHIFT);
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

				memset(&data_store[cursor],
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
				machine_state = MS_ERASE_AWAITING_BLOCK_ADDRESS;
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
void device_init(volatile unsigned long *in_ioregisters, pid_t child_pid)
{
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
		/* Wait until the tracee either hits a watchpoint or terminates. */
		wait(&child_status);

		/* The tracee will trap under four conditions:
		 *  (1) tracee reached the end of its program and terminated,
		 *  (2) tracee hit its gpio_set() breakpoint,
		 *  (3) tracee hit its gpio_get() breakpoint, or
		 *  (4) tracee hit is ioregisters watchpoint.
		 * Use our knowledge of the tracee's breakpoint and watchpoint
		 * configuration to figure out which condition actually happened
		 * and handle it.
		 */
		if (WIFEXITED(child_status))
			break; /* child done, we're done. */

		/* Get tracee's registers to help us figure out what function it
		 * was running when it trapped.
		 */
		ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);

		/* Figure out which breakpoint or watchpoint the tracee hit and
		 * handle it.
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
