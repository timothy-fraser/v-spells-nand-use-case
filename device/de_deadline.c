/*
 * Device emulator deadline module for ready/busy condition tracking.
 *
 * Copyright (c) 2022 Provatek, LLC.
 * Copyright (c) 2023 Timothy Jon Fraser Consulting LLC.
 *
 */

#include <sys/time.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(DIAGNOSTICS_GET) || defined(DIAGNOSTICS_SET)
#include <stdio.h>
#endif

#include "clock.h"
#include "de_deadline.h"

#define MICROSECONDS_IN_SECOND 1000000

static timeus_t deadline;  /* deadline (in microseconds since epoch) */


/* deadline_clear()
 *
 * in:     nothing
 * out:    deadline set by side-effect
 * return: nothing
 *
 * Clears the deadline to 0, making the device ready.
 *
 */

void
deadline_clear(void) {
	deadline = 0;
} /* deadline_clear() */


/* deadline_init()
 *
 * in:     nothing
 * out:    deadline set by side-effect
 * return: nothing
 *
 * Call this function on startup.  Presently this function simply
 * calls deadline_clear() to start the device in a ready state.
 * Retaining this function so that every de_*.c module has an _init()
 * function.
 *
 */

void
deadline_init(void) {
	deadline_clear();
} /* deadline_init() */


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

bool
before_deadline(void) {

	timeus_t timenow = now();

#ifdef DIAGNOSTICS_GET
	if (timenow < deadline) {
		printf("Device busy at time 0x%lX on deadline 0x%lX "
		       "(%lu us).\n", timenow, deadline,
			(deadline - timenow));
	}
#endif
	
	return (timenow < deadline);

}


/* set_deadline()
 *
 * in:     duration - the duration (in microseconds) to add to the current time
 * out:    deadline - set to the current time plus the duration
 * return: nothing
 *
 * Sets the deadline state variable to be the current system time plus the
 * specified duration.
 */

void
set_deadline(timeus_t duration) {

	deadline = now() + duration;

#ifdef DIAGNOSTICS_SET
	printf("Device set deadline 0x%lx (%lu us).\n", deadline, duration);
#endif
	
} /* set_deadline() */
