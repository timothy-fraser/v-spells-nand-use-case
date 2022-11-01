// Copyright (c) 2022 Provatek, LLC.

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/user.h>
#include <time.h>

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

int main()
{
	pid_t child_pid; /* receives what fork() gives us. */

	switch (child_pid = fork())
	{

	case -1: /* fork() failed. */
		perror("Failed to fork");
		exit(-1);

	case 0: /* I am the child. */
		tester_main(&ioregisters);
		break;

	default: /* I am the parent; child_pid holds child pid. */
		device_init(&ioregisters, child_pid);
	}

	/* Both child and parent end up here. */
	exit(0);

} /* main() */
