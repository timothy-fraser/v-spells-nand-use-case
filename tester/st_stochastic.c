/* Copyright (c) 2023 Timothy Jon Fraser LLC */

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "device_emu.h"
#include "framework.h"
#include "st_data.h"
#include "st_mirror.h"
#include "tester.h"


/* The device emulator is surprisingly slow, so we're going to run our
 * tests on a small portion of it that we'll call the "arena".  We'll
 * carefully choose the start and size of this arena so that it wraps
 * around the end of the device emulator's storage, thereby enabling
 * our tests to cover the code paths that involve wrapping the device
 * emulator's cursor.
 */
#define PAGE_SIZE    NUM_BYTES
#define BLOCK_SIZE   (PAGE_SIZE  * NUM_PAGES)
#define DEVICE_SIZE  (BLOCK_SIZE * NUM_BLOCKS)
#define ARENA_SIZE   (4 * BLOCK_SIZE)
#define ARENA_START  (DEVICE_SIZE - (ARENA_SIZE / 2))

#define NUM_OPS 8   /* Each test will have this many operations */

/* These constants support a random choice of read, write, or erase
 * operation where some operations are more likely than others.
 */
#define ODDS_ERASE 1
#define ODDS_WRITE (ODDS_ERASE * 2) /* Writes are twice as common as erases. */
#define ODDS_READ  (ODDS_WRITE * 2) /* Reads are twice as common as writes. */
#define MODULUS (ODDS_ERASE + ODDS_WRITE + ODDS_READ)
#define IS_ERASE(c) (((c) >= 0) && ((c) < ODDS_ERASE))
#define IS_WRITE(c) (((c) >= ODDS_ERASE) && ((c) < (ODDS_ERASE + ODDS_WRITE)))

#define OP_READ  "Read"
#define OP_WRITE "Write"
#define OP_ERASE "Erase"


static unsigned int
random_size(void) {

	unsigned int size = 1 + (random() % ARENA_SIZE);

	assert(size > 1);  /* We don't want any zero-sized operations. */
	assert(size <= ARENA_SIZE);  /* Biggest size uses whole arena. */
	
	return size; 

} /* random_size() */


static unsigned int
random_start(unsigned int size) {

	unsigned int start;

	/* Begin by considering a simple arena whose addresses run
	 * from 0 to ARENA_SIZE-1.  Choose a random start address that
	 * won't cause the operation to run off the end of that simple
	 * arena.
	 */
	start = random() % ((ARENA_SIZE - size) + 1);
	assert((start + size) < ARENA_SIZE);

	/* Now shift the start address to fit our more complicated
	 * actual arena that straddles the end of the NAND storage
	 * device.
	 */
	start += ARENA_START;
	return start;

} /* random_start() */
     


static void
print_op(const char *op, unsigned int first_addr, unsigned int size) {

	first_addr = first_addr % DEVICE_SIZE;
	unsigned int first_block = first_addr / BLOCK_SIZE;
	unsigned int first_page  = (first_addr % BLOCK_SIZE) / PAGE_SIZE;
	unsigned int first_byte  = first_addr % PAGE_SIZE;
	unsigned int last_addr  = (first_addr + size - 1) % DEVICE_SIZE;
	unsigned int last_block = last_addr / BLOCK_SIZE;
	unsigned int last_page  = (last_addr % BLOCK_SIZE) / PAGE_SIZE;
	unsigned int last_byte  = last_addr % PAGE_SIZE;
	
	printf("\t%5s start 0x%06x (first block %3u page %3u byte %3u)\n"
	       "\t       size 0x%06x  (last block %3u page %3u byte %3u)\n",
	       op,
	       first_addr, first_block, first_page, first_byte,
	       size, last_block, last_page, last_byte);

} /* print_op() */


static int
do_erase(unsigned int start, unsigned int size) {

	print_op(OP_ERASE, start, size);
	erase_mirror(start, size);
	if (erase_nand(start, size)) {
		printf("\tDevice timed out on erase operation.\n");
		return -1;
	}
	return 0;

} /* do_erase() */


static int
do_write(unsigned int start, unsigned int size) {

	int ret_val = 0;     /* optimistically presume success */
	unsigned char *buf;  /* buffer of data to write */

	if (!(buf = malloc(size))) {
		printf("\tTest failed to malloc() for write operation.\n");
		return -1;
	}

	data_init(buf, size);
	print_op(OP_WRITE, start, size);
	write_mirror(buf, start, size);
	if (write_nand(buf, start, size)) {
		printf("\tDevice timed out on write operation.\n");
		ret_val = -1;
	}
	free(buf);
	return ret_val;
	
} /* do_write() */


static int
do_read_and_comparison(unsigned int start, unsigned int size) {

	int ret_val = 0;     /* optimistically presume success */
	unsigned char *from_mirror;   /* data read from mirror */
	unsigned char *from_device;   /* data read from device */
	unsigned int i;               /* index into buffers for comparison */

	if(!(from_mirror = malloc(size))) {
		printf("\tTest failed to malloc() for read operation.\n");
		ret_val = -1;
		goto out_nofree;
	}

	if(!(from_device = malloc(size))) {
		printf("\tTest failed to malloc() for read operation.\n");
		ret_val = -1;
		goto out_freemirror;
	}

	print_op(OP_READ, start, size);
	read_mirror(from_mirror, start, size);
	if (read_nand(from_device, start, size)) {
		printf("\tDevice timed out on read operation.\n");
		ret_val = -1;
		goto out_freeboth;
	}

	/* Compare the data we read from the device to the presumably
	 * correct data we read from the mirror.  Indicate an error
	 * and produce some diagnostic output if they do not match.
	 */
	for (i = 0; i < size; i++) {
		if (from_device[ i ] != from_mirror[ i ]) {
			printf("\tData read from device differs from "
			       "data read from mirror\n"
			       "\tat buffer index 0x%08x "
			       "(device index 0x%08x).\n",
			       i, ((start + i) % DEVICE_SIZE));
			printf("Read from device: 0x%02x\n"
			       "Read from mirror: 0x%02x\n",
			       from_device[ i ],
			       from_mirror[ i ]);
			ret_val = -1;
			goto out_freeboth;
		}
	}

	/* If we reach here, we've had a matching read from the mirror
	 * and device and is well.
	 */
	
out_freeboth:
	free(from_device);
out_freemirror:
	free(from_mirror);
out_nofree:
	return ret_val;

} /* do_read_and_comparison() */


static int
do_test(unsigned int arena_start, unsigned int arena_size) {

	unsigned int rwe_start;  /* start address for operations */
	unsigned int rwe_size;   /* size for operations in bytes */
	unsigned int choice;     /* random number that chooses operation */
	unsigned int o;          /* counts operations as we perform them */

	/* Erase entire arena. */
	if (do_erase(arena_start, arena_size)) return -1;

	/* Perform a pseudorandom series of read, write, and erase
	 * operations.
	 */
	for (o = 0; o < NUM_OPS; o++) {

		rwe_size  = random_size();
		rwe_start = random_start(rwe_size);

		/* Make a random choice of read, write, or erase.*/
		choice = random() % MODULUS;
		if (IS_ERASE(choice)) {
			if (do_erase(rwe_start, rwe_size)) return -1;
		} else if (IS_WRITE(choice)) {
			if (do_write(rwe_start, rwe_size)) return -1;
		} else {
			if (do_read_and_comparison(rwe_start, rwe_size))
				return -1;
		}

	}

	/* Framework writes and erases can have arbitrary start
	 * addresses and sizes.  However the device emulator always
	 * writes whole pages.  It zeroes any part of a page involved
	 * in a write outside of the start/size region.  Similarly,
	 * the device emulator always erases whole blocks.  Even
	 * though the start/size region may specify parts of a block,
	 * the device emulator will zero all of every block involved
	 * in an erase.  Read the entire arena to confirm that the
	 * device emulator did all of this extra zeroing correctly,
	 * and also to confirm that it correctly wrote any data that
	 * wasn't lucky enough to be checked already by a read.
	 */
	return do_read_and_comparison(arena_start, arena_size);
		
} /* do_test() */


/* st_stochastic()
 *
 * in:     num_tests - number of tests to run
 * out:    nothing
 * return: 0 if all tests passed, else -1.
 *
 * Run a stochastic ("fuzz") system test on the
 * framework-driver-device system.  num_tests indicates how many
 * random tests to run.
 *
 */

int
st_stochastic(long num_tests) {

	long test;            /* number of the current test 1 ... num_tests */
	time_t time_start;    /* number of seconds since Epoch at test start */
	time_t duration;      /* number of seconds it took to run tests */
	int ret_val = 0;      /* optimistically presume all tests will pass */
	
	time_start = time(NULL);  /* record start time */
	srandom(time_start);      /* seed pseudorandom number generator */
	
	for (test = 1; test <= num_tests; test++) {
		
		printf("Test %ld of %ld:\n", test, num_tests);
		if (do_test(ARENA_START, ARENA_SIZE)) {

			ret_val = -1;  /* Indicate that a test failed. */
			printf("\tTest result: fail.\n\n");

		} else {

			printf("\tTest result: pass.\n\n");

		}

	}
	
	/* If we reach here, test will be the number of tests we
	 * actually ran + 1.  Adjust it to show the number of tests we
	 * actually ran.
	 */
	test--;

	/* Figure out how long it took us to run however many tests we
	 * actually ran and print some time statistics.
	 */
	duration = time(NULL) - time_start;
	printf("Ran %ld tests in %lu seconds (%lf seconds/test).\n",
	       num_tests, duration, ((double)duration / (double)num_tests));
	if (ret_val) {
		printf("At least one test failed.\n");
	} else {
		printf("All tests passed.\n");
	}
	
	return ret_val;
	
} /* st_stochastic() */
