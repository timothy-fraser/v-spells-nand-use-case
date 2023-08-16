/*
 * Device emulator top-level module.
 *
 * Copyright (c) 2022 Provatek, LLC.
 * Copyright (c) 2023 Timothy Jon Fraser Consulting LLC.
 *
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <stddef.h>

#include "framework.h" /* for RIP_IN_GPIO_SET/GET macros */
#include "de_ioregs.h"
#include "de_gpio.h"
#include "de_parser.h"

static volatile unsigned long *ioregs; /* address of ioregisters variable */

/*
 * device_init()
 *
 * in:     in_ioregisters - a pointer to the ioregisters variable from main.c
 *         child_pid      - PID of child tracee
 * out:    none
 * return: none
 *
 * Parent tracer process must call this function on startup.
 * Initializes the device emulator and sets up debugging support for
 * the child process reads and writes to the ioregisters
 * variable. Also initialize breakpoint support for the gpio_set() and
 * gpio_get() functions. Then begins waiting for child process
 * activity.
 */

void
device_init(volatile unsigned long *in_ioregisters, pid_t child_pid) {
	
	int child_status;       /* child process status returned by wait() */
	struct user_regs_struct regs; /* hold tracee register values. */

	ioregs = in_ioregisters;

	/* Wait for first trap. */
	wait(&child_status);

	/* Init ioregisters module.  Set up hardware watchpoint on
	 * child tracee's ioregisters variable so that the child
	 * tracee will trap and pass control to the parent tracer
	 * whenever the child tracee reads or writes ioregisters.
	 */
	ioregs_init(in_ioregisters, child_pid);
	
	/* Init parser and its deadline and store sub-modules. */
	parser_init(in_ioregisters);

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
