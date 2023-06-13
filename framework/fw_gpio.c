#include <assert.h>

#include "framework.h"

/* This is an x86/amd64-specific assembly breakpoint instruction that
 * causes the process that executes it to trap to its debugger.  It
 * uses GCC-specific asm() syntax.  Programs typically do not include
 * this breakpoint instruction in their source; tracers (that is,
 * debuggers) typically insert breakpoints into tracees by modifying
 * their program text, writing this breakpoint instruction to the
 * location where they want the tracee to trap and remembering the
 * original instruction the new breakpoint instruction overwrote so
 * they can restore it and let the tracee continue after they've
 * handled the breakpoint.  Restoring the original instruction clears
 * the breakpoint so the tracee won't trap a second time.  We never
 * want to clear our breakpoint; we always want the tracee to trap, so
 * rather than have the tracer modify the tracee's program text we'll
 * simply make the breakpoint instruction part of the tracee's
 * original program text.
 */

#define BREAKPOINT asm("int $3")

/* gpio_set()
 *
 * in:     pin   - the pin number to set.
 *         value - the value to set, either 0 or non-0.
 * out:    none
 * return: none
 *
 * Traps to the parent tracer, expecting the parent tracer to emulate
 * the following functionality:
 *   If value is 0, clears the GPIO pin indicated by pin.
 *   Otherwise, sets the GPIO pin indicagted by pin.
 *
 * If you modify this function, make sure to adjust the
 * RIP_IN_GPIO_GET() macro.
 */

void
gpio_set(unsigned int pin, unsigned int value) {
	BREAKPOINT;
}

/* gpio_get()
 *
 * in:     pin   - the pin number to get.
 * out:    none
 * return: the value of the pin'th pin, either 0 or 1.
 *
 * Traps to the parent tracer, expecting the parent tracer to emulate
 * the following functionality:
 *   Returns 0 if the pin'th pin is clear, otherwise returns 1.
 *
 * If you modify this function, make sure to adjust the
 * RIP_IN_GPIO_SET() macro.
 */

#define DUMMY 0xAB  /* dummy gpio_get() result to detect breakpoint failure */

unsigned int
gpio_get(unsigned int pin) {
	
	/* It is important that this local variable be an unsigned long.
	 * The tracer will modify the value of this variable using
	 * ptrace(PTRACE_POKEDATA).  The ptrace() function modifies data in
	 * unsigned-long-sized chunks.  If this variable was smaller than an
	 * unsigned long, the tracer's unsigned-long-sized write would
	 * overwrite part of whatever variable was above this one on the
	 * stack.
	 */
	unsigned long retval = DUMMY; /* tracee will modify this value */
	BREAKPOINT;
	assert(retval != DUMMY); /* tracee should have modified retval */
	return retval;

} /* gpio_get() */
