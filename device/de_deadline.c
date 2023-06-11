// Copyright (c) 2022 Provatek, LLC.

/*
 * Routines for emulating the wall-clock time the device spends in a
 * busy state while performing certain operations.
 */

#include <sys/time.h>
#include <stddef.h>
#include <stdbool.h>

#include "de_deadline.h"

#define MICROSECONDS_IN_SECOND 1000000

/* deadline (in microseconds since epoch) */
unsigned long deadline = 0;

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
	
	struct timeval cur_time;

	gettimeofday(&cur_time, NULL);
	unsigned long cur_total = (cur_time.tv_sec * MICROSECONDS_IN_SECOND) +
				  cur_time.tv_usec;

	if (cur_total < deadline)
		return true;

	return false;
}

/*
 * set_deadline()
 *
 * in:  deadline - the current value of the deadline state variable
 *      duration - the duration (in microseconds) to add to the current time
 * out: deadline - set to the current time plus the duration
 * return: nothing
 *
 * Sets the deadline state variable to be the current system time plus the
 * specified duration.
 */

void
set_deadline(int duration) {
	
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	deadline = (cur_time.tv_sec * MICROSECONDS_IN_SECOND) +
		   cur_time.tv_usec + duration;
}
