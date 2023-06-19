// Copyright (c) 2022 Provatek, LLC.

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "device_emu.h"
#include "framework.h"
#include "tester.h"

/* Command-line option flags and an enum to distinguish between modes
 * and bad command line arguments.
 */
#define DETERMINISTIC "--deterministic"
#define STOCHASTIC    "--stochastic"

typedef enum {
	cl_deterministic,
	cl_stochastic,
	cl_error
} cl_t;


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
main(int argc, char * const argv[]) {
	
	pid_t child_pid; /* receives what fork() gives us. */
	struct nand_device *dib_old;    /* DIB before framework/driver init */
	struct nand_device *dib_new;    /* DIB after framework/driver init */
	cl_t mode = cl_error;           /* test mode, default to error */
	long num_tests = 0;             /* count of stochastic tests */
	char *endptr;                   /* strtol()'s end-of-num pointer */
	
	/* Process command-line arguments and set test mode. */
	if (argc == 1) {
		mode = cl_deterministic;
	} else if ((argc == 2) && (!strcmp(argv[ 1 ], DETERMINISTIC))) {
		mode = cl_deterministic;
	} else if ((argc == 3) && (!strcmp(argv[ 1 ], STOCHASTIC))) {

		/* Convert argv[1] to our count of tests.  Indicate
		 * stochastic mode (that is, successful conversion)
		 * only if strtol() left errno 0 indicating that it
		 * converted a number, there are no garbage characters
		 * following the number, and the number is positive.
		 */
		errno = 0;
		num_tests = strtol(argv[2], &endptr, 10);
		if (!errno && (*endptr == '\0') && (num_tests > 0))
			mode = cl_stochastic;
	}
	
	if (mode == cl_error) {
		fprintf(stderr, "USAGE: %s\n", argv[0]);
		fprintf(stderr,	"       %s %s\n", argv[0], DETERMINISTIC);
		fprintf(stderr,	"       %s %s <positive number of tests>\n",
			argv[0], STOCHASTIC);
		return -1;
	}
	
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
		switch (mode) {
			
		case cl_stochastic:
			printf("%ld stochastic tests.\n", num_tests);
			break;
			
		case cl_deterministic:
		default:
			if (st_deterministic()) return -1;

		} /* switch (mode) */

		break;

	default: /* I am the parent; child_pid holds child pid. */
		device_init(&ioregisters, child_pid);
	}

	return 0;

} /* main() */
