// Copyright (c) 2022 Provatek, LLC.

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "device_emu.h"
#include "framework.h"
#include "tester.h"

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


int
main(int argc, const char *argv[]) {
	
	pid_t child_pid; /* receives what fork() gives us. */
	struct nand_device *dib_old;  /* DIB before framework/driver init */
	struct nand_device *dib_new;  /* DIB after framework/driver init */
	
	switch (child_pid = fork()) {

	case -1: /* fork() failed. */
		perror("Failed to fork");
		return -1;

	case 0: /* I am the child. */

		/* Create an initial DIB and then initialize the
		 * framework and whatever driver we've got configured
		 * in the makefiles.  For some drivers, the driver and
		 * the framework simply return the initial DIB
		 * unchanged.  Other drivers (the kilo drivers, for
		 * example) add a new device to the DIB and return the
		 * updated DIB.
		 */
		dib_old = st_dib_init();
		dib_new = init_framework(&ioregisters, dib_old);

		/* For drivers that update the DIB, verify that the
		 * new DIB is correct.
		 */
		if ((dib_old != dib_new) &&           /* if DIB updated ... */
		    (st_dib_test(dib_old, dib_new)))  /* ... verify DIB. */
			return -1;

		/* Run a small set of deterministic system tests. */
		if (st_deterministic()) return -1;

		break;

	default: /* I am the parent; child_pid holds child pid. */
		device_init(&ioregisters, child_pid);
	}

	return 0;

} /* main() */
