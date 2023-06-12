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

/* Macro to get the nth byte from unsigned long ul. */
#define BYTE(ul,n) (((ul) >> ((n)*8)) & 0xFFUL)

/* Macros that recognizes the "movzbl pattern". */
#define MODRM_MOVZBL 7
#define PATTERN_MOVZBL(ul) (  \
	(BYTE(ul, 5) == 0x0F) && \
	(BYTE(ul, 6) == 0xB6) && \
	((BYTE(ul, MODRM_MOVZBL) == 0x00) || \
	 (BYTE(ul, MODRM_MOVZBL) == 0x09) || \
	 (BYTE(ul, MODRM_MOVZBL) == 0x12)))

/* Macros that recognizes the "mov pattern". */
#define MODRM_MOV 3
#define PATTERN_MOV(ul) (  \
	(BYTE(ul, 1) == 0x48) && \
	(BYTE(ul, 2) == 0x8B) && \
	((BYTE(ul, MODRM_MOV) == 0x05) || \
	 (BYTE(ul, MODRM_MOV) == 0x0D) || \
	 (BYTE(ul, MODRM_MOV) == 0x15)))


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

	do {    /* Do once, break on error. Poor man's try/catch. */
		
		/* This sequence of PTRACE_POKEUSER operations sets
		 * one of the four AMD64 hardware debug registers to
		 * be a read-write watchpoint on the ioregister
		 * variable.  I figured out this sequence by running
		 * strace gdb on a similar tracee program and manually
		 * asking gdb to
		 *
                 *   set can-use-hw-watchpoints
                 *   awatch ioregisters
		 *
		 * and looking to see how gdb used ptrace() in the
		 * strace log.
		 */
		
		/* Put the address of the var we want to watch in
		 * debug reg 0.
		 */
		if (ptrace(PTRACE_POKEUSER, child_pid,
			offsetof(struct user, u_debugreg), ioregs))
			break;

		/* Set the watch for read-write. */
		if (ptrace(PTRACE_POKEUSER, child_pid,
			offsetof(struct user, u_debugreg) + 56, 0xF0101))
			break;

		/* Not sure what this is for.  Mask? */
		if (ptrace(PTRACE_POKEUSER, child_pid,
			offsetof(struct user, u_debugreg) + 48, 0))
			break;

		/* done; success. */
		return;
		
	} while (0);

	/* if we reach here, ptrace() failed */
	perror("Failed ptrace watchpoint setup.");
	exit(-1);
	
} /* handle_watchpoint_setup() */


/* error_dump()
 *
 * in:     bytes   - the undecode-able bytes
 *         cause_s - a string describing why we couldn't decode them.
 * out:    Diagnostic messages to console.
 * return: nothing
 *
 * Dumps a diagnostic message containing the bytes we couldn't decode and
 * exits the program.
 *
 */

static void
error_dump(unsigned long bytes, const char *cause_s) {

	/* If we wind up here, we saw bytes that we don't know how
	 * to decode.  Report unhandled instruction text and halt. */
	printf("The driver has used a machine instruction that the device\n"
	       "emulator does not yet understand.\n");
	printf("Cause: %s.\n", cause_s);
	printf("Please include the following program text bytes "
	       "in a bug report:\n");
	printf("Program text: %02lx %02lx %02lx %02lx "
	       "%02lx %02lx %02lx %02lx\n",
	       BYTE(bytes, 0), BYTE(bytes, 1),
	       BYTE(bytes, 2), BYTE(bytes, 3),
	       BYTE(bytes, 4), BYTE(bytes, 5),
	       BYTE(bytes, 6), BYTE(bytes, 7));
	exit(-1);

} /* error_dump() */


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
 * ioregisters variable, it uses some kind of mov instruction that
 * triggers a watchpoint.  That watchpoint pauses the child tracee and
 * activates the parent tracer.  When the parent tracer sees the
 * watchpoint activation, it can update the contents of the
 * ioregisters variable in the child tracee's memory to reflect the
 * value the child tracee ought to have read.  However, this memory
 * update comes too late - the child tracee has already moved whatever
 * value was previously in the ioregisters variable in memory into one
 * of its CPU registers.  This function enables the parent tracer to
 * figure out which CPU register the child tracee used and update its
 * value, as well.
 *
 * This function is essentially a (rather poor) disassembler that
 * knows how to decode the small number of mov instruction patterns
 * that we've seen GCC use to read from the ioregisters variable
 * during our tests plus some variations that we imagine it might use
 * in the future.  We've seen two patterns, described below:
 *
 * The "movzbl pattern":
 *
 * The "movzbl pattern" begins by using a 64-bit mov instruction to
 * load the address of the ioregsiters variable into some register.
 * This mov appears as "48 8b ModR/M" below, where 8b is the mov
 * opcode, 48 is the "REX prefix" indicating 64-bits, and ModR/M
 * stands in for the ModR/M byte that specifies the mov is from a
 * memory location specified relative to RIP to to a particular target
 * register. It indicates the ioregisters variable with a 32-bit
 * constant displacement from RIP, shown as the four DISP bytes below.
 * It takes 7 bytes in all to encode this mov instruction.
 *
 * The "movzbl pattern" then uses a 32-bit movzbl instruction to move
 * the lower 32 bits of the ioregisters variable into that same
 * register, zeroing the higher-order bits.  This movzbl appears as
 * "0f b6" below where 0f is the prefix for two-byte instructions and
 * b6 is the movzbl opcode.  There is also a ModR/M byte specifying
 * the movzbl is from memory specified by the address in the source
 * register to the (same) target register.  This movzbl is the
 * instruction that triggers the watchpoint; it takes 3 bytes in all
 * to encode.
 *
 *   48 8b ModR/M DISP0 DISP1 DISP2 DISP3   "mov"
 *   0f b6 ModR/M                           "movzbl" <- this one traps
 *
 * The "mov pattern":
 *
 * The "mov pattern" is shorter; it uses a mov instruction to load the
 * value of the ioregisters variable directly into a register,
 * triggering the watchpoint.  The mov has the same 7-byte encoding as
 * the mov above; however its ModR/M byte indicates the mov loads the
 * ioregister variable's value from memory into the register rather
 * than its address.
 *
 *   48 8b ModR/M DISP0 DISP1 DISP2 DISP3   "mov"
 *
 * The cdecl Application Binary Interface (ABI) convention for Intel
 * processors allows functions to use RAX, RCX, and REX as
 * general-purpose scratch registers.  In contrast, it demands that
 * functions preserve whatever value the caller left in RBX.  This
 * function handles cases where the mov instructions use RAX, RCX, and
 * REX but not RBX, as these seem to be the patterns GCC is most
 * likely to emit.
 *
 * Furthermore, we handle only forms of mov that *read* from memory,
 * not *write*.
 *
 * We have at least two problems:
 *
 * (1) GCC might emit patterns that use other forms of the mov
 *     instructions and other patterns of register usage.  This
 *     function asserts() when it sees a pattern it cannot disassemble
 *     and barfs out the bytes containing the undecoded instruction.
 *     At that point, there's no choice but to dive back into Volume 2
 *     of the Intel 64 and IA-32 Architectures Software Developer's
 *     Manual and figure out how to decode the new instruction
 *     pattern.
 * 
 * (2) When our watchpoint handler activates, RIP points to the
 *     instruction *after* the one that triggered the watchpoint.  We
 *     need to look at the bytes preceeding RIP to find the mov that
 *     triggered the watchpoint, but since there are many forms of mov
 *     and they have encodings of differing lengths, it *might* be
 *     possible to run into an ambiguous situation.  It *might* be
 *     possible for the four displacement bytes of the seven-byte mov
 *     instruction described above to also happen to decode as a
 *     well-formed three-byte movzbl instruction, for example.
 *
 */

void
update_tracee_cpu_registers(pid_t child_pid,
	struct user_regs_struct *p_regs,
	unsigned int value) {

	unsigned long bytes;   /* bytes containing mov to decode */
	unsigned char modrm;   /* the mov's ModR/M byte value */
	
	/* After watchpoint activation, the traccee's instruction
	 * pointer "rip" points to the instruction *after* the
	 * instruction that triggered the watchpoint.  We want to
	 * examine the instruction that triggered the watchpoint, but
	 * we're not sure how long it is.  We'll peek an unsigned
	 * long's worth of bytes from the tracee's program text that
	 * preceeds where rip points.
	 */
	bytes = ptrace(PTRACE_PEEKDATA, child_pid,
		(p_regs->rip - sizeof(unsigned long)), NULL);

	/* Examine the bytes.  If we can unambiguously recognize the
	 * instruction, pull out the ModR/M byte that will tell us
	 * which register has the value the mov read.  Otherwise, barf
	 * out the undecoded bytes so that we can extend this
	 * disassembler.
	 */
	if (PATTERN_MOVZBL(bytes) && PATTERN_MOV(bytes)) {

		/* We've hit an ambiguous situation where the bytes
		 * could decode to either the movzbl or mov patterns.
		 * Barf some debug output so we can improve the
		 * disassembler.
		 */
		error_dump(bytes, "ambiguous pattern");

	} else if (PATTERN_MOVZBL(bytes)) {

		/* The bytes have the movzbl pattern.  Extract the
		 * ModR/M byte.
		 */
		modrm = BYTE(bytes, MODRM_MOVZBL);

	} else if (PATTERN_MOV(bytes)) {

		/* The bytes have the mov pattern.  Extract the ModR/M
		 * byte.
		 */
		modrm = BYTE(bytes, MODRM_MOV);

	} else {

		/* The bytes do not have a pattern we recognize.  Barf
		 * some debug output so we can improve the
		 * disassembler.
		 */
		error_dump(bytes, "unknown pattern");
		
	}
	
	/* Now that we have the ModR/M byte, use it to determine which
	 * register the mov read the incorrect ioregisters value into
	 * and set that register to the proper value.
	 */
	
	switch (modrm) {
	case 0x00:  /* for movzbl pattern */
	case 0x05:  /* for mov pattern */
		/* destination is AX */
		p_regs->rax = value;
		break;
		
	case 0x09:  /* for movzbl pattern */
	case 0x0D:  /* for mov pattern */
		/* destination is CX */
		p_regs->rcx = value;
		break;
		
	case 0x12:  /* for movzbl pattern */
	case 0x15:  /* for mov pattern */
		/* destination is DX */
		p_regs->rdx = value;
		break;
		
	default:
		/* Should be unreachable if our pattern-recognizing
		 * macros are correct, but better safe than sorry.
		 */
		error_dump(bytes, "unknown ModR/M byte");
	}

	/* Set the tracee's registers to our updated values. */
	ptrace(PTRACE_SETREGS, child_pid, NULL, p_regs);
	return;

} /* update_tracee_cpu_registers() */
