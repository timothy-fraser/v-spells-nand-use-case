/* Unit test for driver nand_wait() functions.
 *
 * Copyright (c) 2023 Timothy Jon Fraser Consulting LLC.
 *
 * This program implements a unit test that demonstrates alpha driver
 * nand_wait() bugs.  Correct nand_wait() functions must time out
 * after some hundreds of microseconds and must spend most of that
 * interval sleeping.  It is difficult for system tests to accurately
 * measure such small time intervals using the device emulator because
 * (1) the nand_wait() function is only one small part of a longer
 * read, write, or erase operation, and (2) the device emulator adds
 * even further computation and timing noise.  It's easier to test
 * these properties with this simple standalone unit test program.
 *
 */

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "clock.h"
#include "device_emu.h"
#include "framework.h"
#include "driver.h"

#define S_TO_US(s) ((s) * 1000000UL)  /* convert seconds to microseconds */
#define TIMEOUT_US (S_TO_US(10UL))       /* test with this timeout value */
#define METATIMEOUT_S 20        /* Fail if not complete in this time (s) */
#define SLEEP_BAR 0.50          /* nand_wait() must sleep at leas this % */

/* The driver expects to link an ioregisters variable.  This one
 * satisfies the linker, but because this simple unit test doesn't use
 * ptrace(), reading or writing this variable doesn't actually cause
 * interaction with a device emulator.
 */
volatile unsigned long ioregisters;

/* gpio_get()
 *
 * in:     pin - ignored
 * out:    nothing
 * return: device busy indication
 *
 * This gpio_get() stub function replaces the framework's function
 * that, in the full system test, would cause interaction with the
 * device emulator.  In this unit test, there is no such interaction
 * and this function always indicates that the (nonexistent) device is
 * busy.
 */
 
unsigned int
gpio_get(unsigned int pin) {
	return DEVICE_BUSY;
}


/* invoke_nand_wait()
 *
 * in:     p_nd - pointer to driver configuration containing jump table
 *         interval_us - interval to wait in microseconds (us).
 * out:    nothing
 * return: 0 on success, -1 on timeout, just as nand_wait() does.
 *
 * This function invokes jump-table-style alpha driver nand_wait()
 * functions.
 *
 */

static int
invoke_nand_wait(struct nand_driver *p_nd, unsigned int interval_us) {

	assert(p_nd->type == NAND_JUMP_TABLE);

	return p_nd->operation.jump_table.wait_ready(interval_us);

} /* invoke_nand_wait() */


/* invoke_nand_set_register()
 *
 * in:     p_nd    - pointer to driver configuration containing jump table
 *         command - NAND command to write to command register
 * out:    writes command to ioregisters variable
 * return: nothing
 *
 * This function invokes jump-table-style alpha driver set_register()
 * functions to trick the driver into thinking it is performing the
 * operation indicated by "command".  This is important for drivers
 * that have nand_wait() bugs that manifest only on certain kinds of
 * operations.
 *
 */

static void
invoke_nand_set_register(struct nand_driver *p_nd, unsigned char command) {

	assert(p_nd->type == NAND_JUMP_TABLE);

	p_nd->operation.jump_table.set_register(IOREG_COMMAND, command);

} /* invoke_nand_set_register() */


/* rusage_to_us()
 *
 * in:     p_ru - rusage structure
 * out:    nothing
 * return: CPU time in microseconds (us).
 *
 * This utility function takes an rusage structure describing process
 * resource usage and returns the sum of the user and system time in
 * microseconds.
 *
 */

static timeus_t
rusage_to_us(struct rusage *p_ru) {

	return  S_TO_US(p_ru->ru_utime.tv_sec) + p_ru->ru_utime.tv_usec +
		S_TO_US(p_ru->ru_stime.tv_sec) + p_ru->ru_stime.tv_usec;
		
} /* rusage_to_us() */


/* handle_metatimeout()
 *
 * in:     signum - receives signal number
 * out:    nothing
 * return: nothing
 *
 * Some drivers have bugs that cause their nand_wait() functions to
 * fail to timeout and hang forever.  Our test program sets an alarm
 * to detect these cases.  When the alarm triggers, this function
 * handles the SIGALRM and we quit waiting for the nand_wait()
 * function to return.  This is a timeout about timeouts - a
 * meta-timeout.
 *
 */
 
static void
handle_metatimeout(int signum) {

	if (signum != SIGALRM) return;
	printf("\tMeasured wall-clock time exceeded %luus.\n",
	       S_TO_US(METATIMEOUT_S));
	printf("Test: confirm nand_wait() indicated timeout: fail.\n");
	printf("At least one test failed.\n");
	exit(-1);

} /* handle_metatimeout() */


/* test_wait()
 *
 * in:     p_driver - pointer to driver configuration and its jumpt table
 *         command  - read, program, or erase command to test
 * out:    nothing
 * return   0 if all tests passed, -1 if at least one test failed.
 *
 * This function tricks the driver's nand_wait() function into
 * thinking it is performing a read, write, or erase operation as
 * indicated by "command" and tests its nand_wait() function on a
 * device that always appears busy.  It performs several specific
 * tests:
 *
 *   - confirm nand_wait() indicates timeout,
 *   - confirm it waited through the proper interval before timing out,
 *   - confirm it spent most of its time sleeping.
 *
 */

static int
test_wait(struct nand_driver *p_driver, unsigned char command) {

	struct sigaction sa;          /* alarm signal handler configuration */
	timeus_t time_start, time_end;   /* start and end time measurements */
	timeus_t time_elapsed;          /* measured wall-clock time elapsed */
	struct rusage ru_start, ru_end;     /* start and end resource usage */
	timeus_t cpu_elapsed;       /* measured user + system CPU time used */
	double sleep_percent;    /* proportion of clock time spent sleeping */
	bool timeout_pass;                /* true iff nand_wait() timed out */
	bool wait_pass;          /* true iff nand_wait() waited long enough */
	bool sleep_pass;     /* true iff nand_wait() slept most of the time */
	int ret_val;                 /* holds value returned by nand_wait() */

	/* Some drivers have bugs that manifest only for particular
	 * operations.  Use nand_set_register() to trick the driver
	 * into thinking it is working on a read, program, or erase
	 * operation.
	 */
	invoke_nand_set_register(p_driver, command);
	
	/* Set an alarm.  If a buggy nand_wait() hangs, declared test
	 * failure after the alarm goes off.
	 */
	sa.sa_handler = handle_metatimeout;
	if (sigaction(SIGALRM, &sa, NULL)) {
		perror("failed to set signal hander.\n");
		exit(-1);
	}
	alarm(METATIMEOUT_S);
	
	/* Get resource usage, wall-clock time just before nand_wait(). */
	if (getrusage(RUSAGE_SELF, &ru_start)) {
		perror("failed to get resource usage.\n");
		exit(-1);
	}
	time_start = now();

	/* Execute nand_wait(). */
	ret_val = invoke_nand_wait(p_driver, TIMEOUT_US);

	/* Get resource usage, wall-clock time just after nand_wait(). */
	time_end = now();
	if (getrusage(RUSAGE_SELF, &ru_end)) {
		perror("failed to get resource usage.\n");
		return -1;
	}

	/* nand_wait() did not hang.  Clear alarm. */
	alarm(0);
	
	/* Test: did nand_wait() time out? */
	timeout_pass = (ret_val == -1);
	
	/* Test: did nand_wait() wait long enough before timing out? */
	time_elapsed = time_end - time_start;
	wait_pass = (time_elapsed >= TIMEOUT_US);
	
	/* Test: did nand_wait() spend most of its time sleeping? */
	cpu_elapsed = rusage_to_us(&ru_end) - rusage_to_us(&ru_start);
	sleep_percent = 1.0 - ((double)cpu_elapsed / (double)time_elapsed);
	sleep_pass = (sleep_percent > SLEEP_BAR);

	/* Output results to console. */
	printf("\tMeasured wall-clock time:  %8luus.\n", time_elapsed);
	printf("\tMeasured CPU time:         %8luus.\n", cpu_elapsed);
	printf("\tComputed sleep percent:    %8.0f%%.\n",
	       (sleep_percent * 100.0));

	printf("\tTest: confirm nand_wait() indicated timeout: "
	       "              %s.\n", (timeout_pass ? "pass" : "fail"));
	printf("\tTest: confirm nand_wait() waited at least %luus: "
	       "     %s.\n", TIMEOUT_US, (wait_pass ? "pass" : "fail"));
	printf("\tTest: confirm nand_wait() slept more than %.0f%% "
	       "of the wait: %s.\n", (SLEEP_BAR * 100.0),
	       (sleep_pass ? "pass" : "fail"));

	return (timeout_pass && wait_pass && sleep_pass ? 0 : -1);

} /* test_wait() */


int
main(void) {

	struct nand_driver driver;    /* contains jump table or interpreter */
	int ret_val = 0;              /* optimistically presume success */
	
	/* Set up driver. */
	init_nand_driver(&ioregisters, NULL);
	driver = get_driver();

	printf("Executing nand_wait(%luus) on busy device for "
	       "read operation.\n", TIMEOUT_US);
	if (test_wait(&driver, C_READ_EXECUTE)) ret_val = -1;

	printf("Executing nand_wait(%luus) on busy device for "
	       "write operation.\n", TIMEOUT_US);
	if (test_wait(&driver, C_PROGRAM_EXECUTE)) ret_val = -1;

	printf("Executing nand_wait(%luus) on busy device for "
	       "erase operation.\n", TIMEOUT_US);
	if (test_wait(&driver, C_ERASE_EXECUTE)) ret_val = -1;

	if (ret_val) {
		printf("At least one test failed.\n");
	} else {
		printf("All tests passed.\n");
	}
	return ret_val;
	
} /* main() */
