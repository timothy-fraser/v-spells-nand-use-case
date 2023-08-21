#ifndef _CLOCK_H_
#define _CLOCK_H_

/* The following type is for time in microseconds since the epoch.
 * We're defining our own type rather than using the Linux suseconds_t
 * microseconds type because suseconds_t is guaranteed to be large
 * enough to count only up to one second, and we want to store larger
 * counts.
 */
typedef unsigned long timeus_t;

timeus_t now(void);

#endif
