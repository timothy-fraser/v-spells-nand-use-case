/* Copyright (c) 2023 Timothy Jon Fraser Consulting LLC 
 *
 * This module implements a mirror of the NAND device emulator's
 * storage that does not rely on the framework-driver-device system.
 * System tests can use this mirror to track the effects read, write,
 * and erase operations *should* be having on the NAND device
 * emulator's storage if the framework-driver-device software stack is
 * correct.  Their test oracles can then use it as an "answer key" for
 * comparing the results of long series of read, write, and erase
 * operations.
 */

#include <sys/types.h>
#include <assert.h>
#include <stdio.h>

#include "device_emu.h"
#include "st_mirror.h"

/* The following constants, macros, and globals define the primary
 * data structure for this module: a region of memory in which the
 * module stores the data that ideally ought to be in the NAND device
 * emulator's storage - the "answer key" for framework-driver-device
 * system tests.  
 *
 * Peculiar behavior: if a caller attempts to read, write, or erase
 * off the end of this storage, it's not an error.  Instead, the read,
 * write, or erase will continue from the beginning of storage ("wrap").
 *
 */

#define PAGE_SIZE     NUM_BYTES
#define BLOCK_SIZE    (NUM_PAGES * NUM_BYTES)
#define MIRROR_SIZE   (NUM_BLOCKS * NUM_PAGES * NUM_BYTES)
#define WRAP(i)       ((i) % MIRROR_SIZE)
#define PAGE_START(i) (((unsigned int)((i) / NUM_BYTES)) * NUM_BYTES)
static unsigned char  mirror[MIRROR_SIZE];


/* read_mirror()
 *
 * in:     offset - read starting from this offset (wrapped to mirror size)
 *         size   - read this many bytes
 * out:    buffer - bytes read copied to this buffer
 * return: nothing
 *
 * Reads the bytes specified by offset and size from the mirror to buffer.
 *
 */

void
read_mirror(unsigned char *buffer, unsigned int offset, unsigned int size) {

	unsigned int m;  /* index into mirror */
	unsigned int b;  /* index into buffer */
	
	for (b = 0, m = WRAP(offset); b < size; b++, m = WRAP(m + 1)) {
		assert(m < MIRROR_SIZE);
		buffer[ b ] = mirror[ m ];
	}
	
} /* read_mirror() */


/* write_mirror()
 *
 * in:     buffer - array containing bytes to write to mirror
 *         offset - begin writing at this offset
 *         size   - write this many bytes
 * out:    mirror - written by side-effect.
 * return: nothing
 *
 * Writes size bytes from buffer to mirror beginning at offset.
 *
 * Peculiar behavior: the NAND device emulator always writes whole
 * pages.  If the write does not begin precisely at the beginning of
 * its first page, this function will write additional zeroes from the
 * beginning of the first page up to where the actual data begins.
 * Similarly, if the write does not fill all of its last page, this
 * function will write additional zeroes from just after the last
 * actual data byte to the end of the last page.
 *
 */

void
write_mirror(unsigned char *buffer, unsigned int offset, unsigned int size) {

	unsigned int m;  /* index into mirror */
	unsigned int b;  /* index into buffer */

	/* Zero the first page preceeding the first actual data byte.
	 * When this loop exits, the mirror index m is set to begin
	 * writing actual data bytes to the mirror.
	 */
	offset = WRAP(offset);
	for (m = PAGE_START(offset); m < offset; m++) {
		assert(m < MIRROR_SIZE);
		mirror[ m ] = 0;
	}
	
	/* Write the actual data bytes. When this loop exits, the
	 * mirror index m is set to begin writing zeroes to the last
	 * page.
	 */
	for (b = 0; b < size; b++, m = WRAP(m + 1)) {
		assert(m < MIRROR_SIZE);
		mirror[ m ] = buffer[ b ];
	}
	
	
	/* Zero the last page beyond the last actual data byte. */
	for (; (m % PAGE_SIZE) != 0; m = WRAP(m + 1)) {
		assert(m < MIRROR_SIZE);
		mirror[ m ] = 0;
	}
	
} /* write_mirror() */



/* erase_mirror()
 *
 * in:     offset - begin erasing at the block containing this byte offset
 *         size   - erase blocks until you've covered all blocks containing
 *                  at least part of the byte range (offset + size).
 * out:    mirror - erased by side-effect.
 * return: nothing
 *
 * Erases a contiguous series of complete blocks that contains the
 * byte range that starts at offset and ends at (offset + size).
 *
 * Peculiar behavior: the NAND device emulator always erases whole
 * blocks.  If the erase does not begin precisely at the beginning of
 * its first block, this function will write additional zeroes from
 * the beginning of the first block up to where the specified byte
 * range begins.  Similarly, if the specified byte range does not
 * cover all of its last block, this function will write additional
 * zeroes from just after the end of the byte range to the end of the
 * last block.
 *
 */

void
erase_mirror(unsigned int offset, unsigned int size) {

	unsigned int m;  /* index into mirror */

	/* Make offset point to the beginning of the first
	 * block and adjust size accordingly.
	 */
	offset = WRAP(offset);
	size += offset % BLOCK_SIZE;
	offset = ((unsigned int)(offset / BLOCK_SIZE)) * BLOCK_SIZE;
	
	/* We've block-aligned the start of the region.  Now increase
	 * size as needed to cover the entire last block. */
	size += (BLOCK_SIZE - (size % BLOCK_SIZE));

	/* Perform the erasure. */
	for (m = offset; m < size; m = WRAP(m + 1)) {
		assert(m < MIRROR_SIZE);
		mirror[ m ] = 0;
	}
	
} /* erase_mirror() */


#ifdef UNIT_TEST

/* This UNIT_TEST code implements a unit test for this module.
 * Compile with -DUNIT_TEST to test this module in isolation. Don't
 * use -DUNIT_TEST when compiling this module for inclusion in the
 * tester library.
 */

#include "st_data.h"

/* Set up a write-read-erase-read test to cover these cases:
 *   - write-read-erase region start is not block or page aligned,
 *   - region end is not block or page aligned,
 *   - region wraps around the end of the mirror, and
 *   - offset is beyond end of mirror and must also be wrapped.
 */
#define TEST_SIZE   BLOCK_SIZE
#define TEST_OFFSET (((2 * MIRROR_SIZE) - BLOCK_SIZE) + PAGE_SIZE + 7)
#define NONZERO_CHAR 'x'   /* a byte value data_print() will print */

int
main(int argv, char *argc[]) {

	unsigned char data_written[ TEST_SIZE ];
	unsigned char data_read[ TEST_SIZE ];
	unsigned int i;
	unsigned int true_start;  /* index of start of first page touched */
	unsigned int true_end;    /* index of end of last page touched */
	
	/* Set entire mirror to non-zero value so that we can confirm
	 * later zeroization works properly.
	 */
	for (i = 0; i < MIRROR_SIZE; i++) {
		mirror[ i ] = NONZERO_CHAR;
	}

	true_start = ((unsigned int)(WRAP(TEST_OFFSET) / PAGE_SIZE))
		* PAGE_SIZE;
	true_end = ((((unsigned int)(WRAP(TEST_OFFSET + TEST_SIZE)
		/ PAGE_SIZE)) + 1) * PAGE_SIZE) - 1;
	
	printf("Test: store and retrieve %u bytes to mirror index 0x%08x,\n"
	       "      true start index 0x%08x block %u page %u offset %u,\n"
	       "        true end index 0x%08x block %u page %u offset %u.\n\n",
	       TEST_SIZE, TEST_OFFSET,
	       true_start, (true_start / BLOCK_SIZE),
	       ((true_start % BLOCK_SIZE) / PAGE_SIZE),
	       (true_start % PAGE_SIZE),
	       true_end, (true_end / BLOCK_SIZE),
	       ((true_end % BLOCK_SIZE) / PAGE_SIZE),
	       (true_end % PAGE_SIZE));
	
	puts("Writing data...");
	data_init(data_written, TEST_SIZE);
	data_print(data_written, TEST_SIZE);
	write_mirror(data_written, TEST_OFFSET, TEST_SIZE);
	
	puts("\nReading data (ideally, identical)...");
	read_mirror(data_read, TEST_OFFSET, TEST_SIZE);
	data_print(data_read, TEST_SIZE);

	if (TEST_SIZE != (i = data_compare(data_written, data_read,
		TEST_SIZE))) {
		printf("\nFail - buffers differ at index 0x%08x.\n", i);
		return -1;
	}
	
	/* confirm prefix of first page zeroed. */
	for (i = true_start; i < WRAP(TEST_OFFSET); i++) {

		assert(i < MIRROR_SIZE);
		if (mirror[ i ] != 0) {
			printf("\nFail - nonzero prefix data at index "
				"0x%08x.\n", i);
			return -1;
		}
	}
	
	/* confirm postfix of last page zeroed. */
	for (i = WRAP(TEST_OFFSET + TEST_SIZE);
		i <= true_end; i++) {

		assert(i < MIRROR_SIZE);
		if (mirror[ i ] != 0) {
			printf("\nFail - nonzero postfix data at index "
				"0x%08x.\n", i);
			return -1;
		}
	}
	puts("Pass - confirmed data read matched data written,\n"
	     "prefix of first page zeroed, and\n"
	     "postfix of last page zeroed.\n");
	
	true_start = ((unsigned int)(WRAP(TEST_OFFSET) / BLOCK_SIZE))
		* BLOCK_SIZE;
	true_end = ((((unsigned int)(WRAP(TEST_OFFSET + TEST_SIZE)
		/ BLOCK_SIZE)) + 1) * BLOCK_SIZE) - 1;
	
	printf("Test: erase a range of %u bytes starting at index 0x%08x,\n"
	       "      true start index 0x%08x block %u page %u offset %u,\n"
	       "        true end index 0x%08x block %u page %u offset %u.\n\n",
	       TEST_SIZE, TEST_OFFSET,
	       true_start, (true_start / BLOCK_SIZE),
	       ((true_start % BLOCK_SIZE) / PAGE_SIZE),
	       (true_start % PAGE_SIZE),
	       true_end, (true_end / BLOCK_SIZE),
	       ((true_end % BLOCK_SIZE) / PAGE_SIZE),
	       (true_end % PAGE_SIZE));
	
	erase_mirror(TEST_OFFSET, TEST_SIZE);

	/* confirm entire range of complete blocks zeroed. */
	for (i = true_start; i <= true_end; i++) {

		assert(WRAP(i) < MIRROR_SIZE);
		if (mirror[ WRAP(i) ] != 0) {
			printf("Fail - nonzero erased data at index "
				"0x%08x.\n", i);
			return -1;
		}
	}

	puts("Pass - confirmed entire range of complete blocks zeroed.\n");
	return 0;
	
} /* main() */

#endif

