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
#define PAGE_SIZE          NUM_BYTES
#define BLOCK_SIZE         (NUM_PAGES * NUM_BYTES)
#define MIRROR_SIZE        (NUM_BLOCKS * NUM_PAGES * NUM_BYTES)

/* The following three macros return the number of the block, the
 * number of the page within its block, and the number of the byte
 * within its page of the offset o, respectively.
 */
#define BLOCK(o)        ((o) / BLOCK_SIZE)
#define PAGE(o)         (((o) % BLOCK_SIZE) / PAGE_SIZE)
#define BYTE(o)         ((o) % PAGE_SIZE)

/* The following four macros compute the offset to the first byte or
 * last byte in the page or block that contains offset o.
 */
#define PAGE_START(o)  (((o) / PAGE_SIZE) * PAGE_SIZE)
#define PAGE_END(o)    (PAGE_START(o) + PAGE_SIZE - 1)
#define BLOCK_START(o) (((o) / BLOCK_SIZE) * BLOCK_SIZE)
#define BLOCK_END(o)   (BLOCK_START(o) + BLOCK_SIZE - 1)

/* This module's functions accept offsets that are arbitrary unsigned
 * ints, but the mirror itself has a relatively small size.  This
 * macro wraps offsets to the size of the mirror.  Convention: don't
 * wrap until you actually need to index an array or print.  Wrapping
 * makes it hard to compare offsets with "<" and "<=".
 */
#define WRAP(o) ((o) % MIRROR_SIZE)

static unsigned char mirror[MIRROR_SIZE];


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

	unsigned int i;  /* index into both mirror and buffer */

	for (i = 0; i < size; i++) {
		buffer[ i ] = mirror[ WRAP(offset + i) ];
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

	unsigned int i;  /* index into both mirror and buffer */

	/* Zero the first page preceeding the first actual data byte. */
	for (i = PAGE_START(offset); i < offset; i++) {
		mirror[ WRAP(i) ] = 0;
	}

	/* Write the actual data bytes. */
	for (i = 0; i < size; i++) {
		mirror[ WRAP(offset + i) ] = buffer[ i ];
	}

	/* Zero the last page beyond the last actual data byte. */
	for (i = offset + size; i <= PAGE_END(offset + size - 1); i++) {
		mirror[ WRAP(i) ] = 0;
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

	for (m = BLOCK_START(offset); m <= BLOCK_END(offset + size - 1); m++) {
		mirror[ WRAP(m) ] = 0;
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

static unsigned char data_written[ MIRROR_SIZE ];
static unsigned char data_read[ MIRROR_SIZE ];

static int
test(unsigned int offset, unsigned int size) {

	unsigned int i;
	unsigned int true_start;  /* index of start of first page touched */
	unsigned int true_end;    /* index of end of last page touched */
	
	/* Set entire mirror to non-zero value so that we can confirm
	 * later zeroization works properly.
	 */
	for (i = 0; i < MIRROR_SIZE; i++) {
		mirror[ i ] = NONZERO_CHAR;
	}

	true_start = PAGE_START(offset);
	true_end = PAGE_END(offset + size - 1);
	
	printf("Test: store and retrieve 0x%06x bytes "
	       "to mirror index 0x%08x,\n"
	       "      true start index 0x%08x "
	       "block %03u page %03u offset %03u,\n"
	       "        true end index 0x%08x "
	       "block %03u page %03u offset %03u.\n",
	       size, offset,
	       true_start,
	       BLOCK(WRAP(true_start)), PAGE(true_start), BYTE(true_start),
	       true_end,
	       BLOCK(WRAP(true_end)), PAGE(true_end), BYTE(true_end));
	
	puts("      Writing data...");
	data_init(data_written, size);
	write_mirror(data_written, offset, size);
	
	puts("      Reading data (ideally, identical)...");
	read_mirror(data_read, offset, size);

	if (size != (i = data_compare(data_written, data_read, size))) {
		printf("      Fail - buffers differ at index 0x%06x.\n", i);
		printf("written: 0x%02x%02x%02x%02x\n", data_written[0],
			data_written[1], data_written[2], data_written[3]);
		printf(" mirror: 0x%02x%02x%02x%02x\n", mirror[0], mirror[1],
			mirror[2], mirror[3]);
		printf("   read: 0x%02x%02x%02x%02x\n", data_read[0],
			data_read[1], data_read[2], data_read[3]);
		return -1;
	}
	
	/* confirm prefix of first page zeroed. */
	for (i = true_start; i < offset; i++) {

		if (mirror[ WRAP(i) ] != 0) {
			printf("      Fail - nonzero prefix data at index "
				"0x%08x.\n", i);
			return -1;
		}
	}
	
	/* confirm postfix of last page zeroed. */
	for (i = offset + size; i <= true_end; i++) {

		if (mirror[ WRAP(i) ] != 0) {
			printf("      Fail - nonzero postfix data at index "
				"0x%08x.\n", i);
			return -1;
		}
	}
	puts("      Pass - confirmed data read matched data written,\n"
	     "             prefix of first page zeroed, and "
	     "postfix of last page zeroed.\n");

	true_start = BLOCK_START(offset);
	true_end   = BLOCK_END(offset + size - 1);
	
	printf("Test: erase a range of 0x%06x bytes "
	       "starting at index 0x%08x,\n"
	       "      true start index 0x%08x "
	       "block %03u page %03u offset %03u,\n"
	       "        true end index 0x%08x "
	       "block %03u page %03u offset %03u.\n",
	       size, offset,
	       true_start,
	       BLOCK(WRAP(true_start)), PAGE(true_start), BYTE(true_start),
	       true_end,
	       BLOCK(WRAP(true_end)), PAGE(true_end), BYTE(true_end));
	
	erase_mirror(offset, size);

	/* confirm entire range of complete blocks zeroed. */
	for (i = true_start; i <= true_end; i++) {

		if (mirror[ WRAP(i) ] != 0) {
			printf("      Fail - nonzero erased data at index "
				"0x%08x.\n", i);
			return -1;
		}
	}

	puts("      Pass - confirmed entire range of complete blocks "
	     "zeroed.\n");
	return 0;

} /* test() */


int
main(int argv, char *argc[]) {

	if (test(TEST_OFFSET, TEST_SIZE)) return -1;
	if (test(0, MIRROR_SIZE)) return -1;
	return 0;
	
} /* main() */

#endif

