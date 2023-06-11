// Copyright (c) 2022 Provatek, LLC.

/*
 * Routines for emulating General-Purpose Input-Output (GPIO) pins.
 */

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdbool.h>

#include "device_emu.h"
#include "de_gpio.h"
#include "de_deadline.h"
#include "de_device.h"

#define RESET_DURATION 500          /* reset duration in microseconds */

extern unsigned int machine_state;  /* device emulator state machine state */

/*
 * handle_breakpoint_gpio_set()
 *
 * in:  p_regs - pointer to register struct containing tracee's register values
 * out: machine_state - reset to the initial state if reset pin is set 
 * return: nothing
 *
 * This function processes tracee calls to its gpio_set() function.
 */

void
handle_breakpoint_gpio_set(struct user_regs_struct *p_regs) {

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

void
handle_breakpoint_gpio_get(pid_t child_pid, struct user_regs_struct *p_regs) {
	
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
