/* 
 * Monotonically increasing microsecond resolution clock module.
 *
 * Copyright (c) 2023 Timothy Jon Fraser Consuling LLC.
 *
 * This module provides a monotonically increasing microsecond
 * resolution clock.  It's now() function returns a count of
 * microseconds ("usecs") since the epoch.  Each call to now() will
 * return a value that is either the same as the previous call or
 * greater.
 *
 */

#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "clock.h"

/* Macros for converting between various units of time. */
#define  S_TO_US(s)   ((s) * (timeus_t)1000000)
#define NS_TO_US(ns) ((ns) / (timeus_t)1000)


/* now()
 *
 * in:     nothing
 * out:    nothing
 * return: time in microseconds ("usecs").
 *
 * Returns the present time in terms of microseconds since the epoch.
 * Each call will return a value that is at least as large as the
 * previous call.
 *
 */

timeus_t
now(void) {
	
	struct timespec ts;  /* C library's representation of now */

	if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
		perror("Failed to get current time");
		exit(-1);
	}

	return S_TO_US(ts.tv_sec) + NS_TO_US(ts.tv_nsec);

} /* now() */


