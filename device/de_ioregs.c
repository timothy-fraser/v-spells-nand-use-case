// Copyright (c) 2022 Provatek, LLC.

/*
 * Routines to emulate device Input-Output (IO) Registers.
 */

#include <sys/types.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "de_ioregs.h"

/* Address of the ioregisters variable.  The actual ioregister
 * variable is defined in main.c.
 */
volatile unsigned long *ioregs;


/* handle_watchpoint_setup()
 *
 * in:     child_pid - the PID of the child tracee
 * out:    none
 * return: none
 *
 * Sets up read-write watchpoint on the tracee's ioregisters variable.
 *
 */

void
handle_watchpoint_setup(pid_t child_pid) {
	
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

void
update_tracee_cpu_registers(pid_t child_pid,
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
			printf("Unhandled flavor of movz.\n");
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
	printf("Program text: %02lx %02lx %02lx %02lx "
	       "%02lx %02lx %02lx %02lx\n",
	       BYTE(instructions, 0), BYTE(instructions, 1),
	       BYTE(instructions, 2), BYTE(instructions, 3),
	       BYTE(instructions, 4), BYTE(instructions, 5),
	       BYTE(instructions, 6), BYTE(instructions, 7));
	exit(-1);

} /* update_tracee_cpu_registers() */
