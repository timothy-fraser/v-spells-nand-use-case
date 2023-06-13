/*
 * This program implements a unit test for de_ioregs.c, in particular
 * its disassembler/instruction decoder.
 *
 */

#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>

#include "de_ioregs.h"
#include "test_patterns.h"

/* Test logic:
 *
 * We'll run a series of tests.  Each test will run through the
 * following sequence of steps using different mov pattern details.
 * We'll aim to have a test for every mov pattern our instruction
 * decoder supports.  The steps:
 *
 *         Child/tracee                      Parent/tracer
 *
 * (1) Mov (write) VALUE_INCORRECT
 *     into ioregisters using C.
 *                                   (2) Ignore this mov; it's
 *                                       a write.
 * (3) Mov (read) contents of
 *     ioregisters using a
 *     particular pattern of
 *     mov instruction and
 *     register.
 *                                   (4) Use update_tracee_cpu_
 *                                       registers() to change
 *                                       the value the child/tracee
 *                                       read to VALUE_CORRECT.
 * (5) Confirm the value read
 *     is VALUE_CORRECT and not
 *     VALUE_INCORRECT.
 * 
 */

#define VALUE_INCORRECT 0x0123456789ABCDEFULL
#define VALUE_CORRECT   0xAA


/* Emulated Input-Output registers covered by watchpoint.
 * 'ioregisters' is the actual variable, defined here.  The
 * de_ioregs.c module has its own global 'ioregs' that we'll set to
 * the *address* of 'ioregisters'.
 */
volatile unsigned long ioregisters = 0;  /* The IO registers variable. */
extern volatile unsigned long *ioregs;   /* It's address, from de_ioregs.c */

/* The parent/tracer will use this flag to track the child/tracee's
 * alternation between ioregisters writes (false) and ioregisters
 * reads (true).
 */
static bool is_read = false;

/* tracee()
 *
 * in:     nothing
 * out:    ioregisters - overwritten with test values
 * return: nothing
 *
 * This function implements the child/tracee half of our test logic.
 * It calls out to some assembly routines to do the mov's with
 * specific instructions and registers.
 *
 * Writes pass/fail test results to console.
 *
 */

void
tracee(void) {

	unsigned int value;  /* value read from ioregisters */
	
	ioregisters = VALUE_INCORRECT;
	value = pattern_movzbl_ax();
	printf("Pattern movzbl AX: %s.\n",
	       (value == VALUE_CORRECT ? "pass" : "fail"));

	ioregisters = VALUE_INCORRECT;
	value = pattern_movzbl_cx();
	printf("Pattern movzbl CX: %s.\n",
	       (value == VALUE_CORRECT ? "pass" : "fail"));

	ioregisters = VALUE_INCORRECT;
	value = pattern_movzbl_dx();
	printf("Pattern movzbl DX: %s.\n",
	       (value == VALUE_CORRECT ? "pass" : "fail"));

	ioregisters = VALUE_INCORRECT;
	value = pattern_mov_ax();
	printf("Pattern mov AX: %s.\n",
	       (value == VALUE_CORRECT ? "pass" : "fail"));

	ioregisters = VALUE_INCORRECT;
	value = pattern_mov_cx();
	printf("Pattern mov CX: %s.\n",
	       (value == VALUE_CORRECT ? "pass" : "fail"));
	
	ioregisters = VALUE_INCORRECT;
	value = pattern_mov_dx();
	printf("Pattern mov DX: %s.\n",
	       (value == VALUE_CORRECT ? "pass" : "fail"));
	
} /* tracee() */


/* handle_watchpoint()
 *
 * in:     child_pid - PID of child/tracee
 * out:    is_read, child/tracee register state updated via side-effect
 * return: nothing
 *
 * This function implements the parent/tracer side of the test steps,
 * including it's alternation between (2) ignoring ioregisters writes
 * and (4) changing the value of ioregisters reads.
 */

static void
handle_watchpoint(pid_t child_pid) {

	struct user_regs_struct regs;  /* child/tracee register state */

	if (is_read) {

		/* Handle reads to ioregisters. */
		
		/* Poke (write) the new test value we want the child
		 * tracee to see in its ioregisters variable.
		 */
		ptrace(PTRACE_POKEDATA, child_pid, ioregs, VALUE_CORRECT);

		/* Get the child/tracee's CPU register state and then
		 * update it so that it also shows the new test value.
		 */
		ptrace(PTRACE_GETREGS, child_pid, NULL, &regs);
		update_tracee_cpu_registers(child_pid, &regs, VALUE_CORRECT);
		
		is_read = false;  /* The next trap will be a write. */

	} else {

		/* Ignore ioregisters writes. */
		is_read = true;   /* The next trap will be a read. */

	}

} /* handle_watchpoint() */


/* tracer()
 *
 * in:     child_pid   - PID of child tracee process
 * out:    ioregisters - set to test values
 * return: nothing
 *
 * This function implements the parent/tracer half of our test logic.
 *
 */

static void
tracer(pid_t child_pid) {

	int child_status;      /* child process status returned by wait() */

	/* Wait for child/tracee's first trap. */
	wait(&child_status);
	
	/* Make de_ioregs.c's ioregs variable contain the address of
	 * the ioregisters variable.  Set a CPU watchpoint to watch
	 * ioregisters.
	 */
	ioregs = &ioregisters;
	handle_watchpoint_setup(child_pid);
	
	/* Setup complete.  Let tracee continue. */
	ptrace(PTRACE_CONT, child_pid, NULL, NULL);

	/* Parent tracer loops handling traps until the child tracee exits. */
	while (1) {

		/* Wait until the tracee either hits a watchpoint or
		 * terminates.
		 */
		wait(&child_status);
		if (WIFEXITED(child_status)) 
			break; /* child done, we're done. */
		
		handle_watchpoint(child_pid);


		/* Let the tracee continue. */
		ptrace(PTRACE_CONT, child_pid, NULL, NULL);
	}
}



int
main(int argc, char *argv[]) {
	
	pid_t child_pid;          /* child PID or fork() error code */

	switch (child_pid = fork()) {

	case -1: /* fork() failed. */
		perror("Failed to fork");
		return -1;

	case 0:
		/* I am the child.  Initiate a trace.  Parent is the
		 * tracer, child is the tracee.
		 */
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);

		/* Get my own pid so I can pause myself and let the
		 * parent/tracer set up its watchpoint.
		 */
		child_pid = getpid();  
		kill(child_pid, SIGTRAP);  /* Pause myself. */

		tracee();
		break;

	default: /* I am the parent; child_pid holds child pid. */
		tracer(child_pid);
	}

	/* Both child and parent end up here. */
	return 0;
	
} /* main() */
