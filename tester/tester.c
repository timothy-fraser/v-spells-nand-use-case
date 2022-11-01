// Copyright (c) 2022 Provatek, LLC.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"

#define STORAGE_ADDR 0

/* These constants define the amount of data we'll be storing and
 * retrievig from the device in terms of rows and columns of
 * characters we can neatly print to the console.
 */
#define DATA_ROWS    5
#define DATA_COLUMNS 60
#define DATA_SIZE    (DATA_ROWS * DATA_COLUMNS)

/* Macros for determining if a number corresponds to a character we
 * think is printable and for mapping numbers to printable characters.
 */
#define IS_PRINTABLE(i) ('a' <= (i) && (i) <= 'z')
#define TO_PRINTABLE(i) ('a' + (i) % ('z' - '`'))


static unsigned char data[DATA_SIZE];
static unsigned char dest[DATA_SIZE];


/* data_init()
 *
 * in:     length - buffer length in bytes
 * out:    buffer - will be filled with printable characters
 * return: nothing
 *
 * Fill the indicated buffer with printable characters.
 *
 */

static void data_init(unsigned char *buffer, unsigned int length) {

	unsigned int i;    /* index into buffer */

	for (i = 0; i < length; i++) {
		buffer[ i ] = TO_PRINTABLE(i);
	}

} /* data_init() */


/* data_print()
 *
 * in:     length - buffer length in bytes
 *         buffer - buffer to print
 * out:    nothing
 * return: nothing
 *
 * Print the contents of buffer to stdout.  Show non-printable
 * characters as '-'.
 *
 */

static void data_print(unsigned char *buffer, unsigned int rows,
	unsigned int columns) {

	unsigned int r, c;    /* row, column indicies into buffer */
	unsigned char b;      /* byte to print */
	
	for (r = 0; r < rows; r++) {
		for(c = 0; c < columns; c++) {
			b = buffer[ r * columns + c ];
			if (IS_PRINTABLE(b)) {
				putchar(b);
			} else {
				putchar('-');
			}
		}
		putchar('\n');
	}
	putchar('\n');
	
} /* data_print() */


/* data_compare()
 *
 * in:     length - length of both buffers in bytes
 *         b1     - buffer to compare
 *         b2     - buffer to compare
 * out:    nothing
 * return: nothing
 *
 * Compare the buffers, return if identical, exit(-1) if different.
 *
 */

static void data_compare(unsigned char *b1, unsigned char *b2,
	unsigned int length) {

	unsigned int i;    /* index into buffers */

	for (i = 0; i < length; i++) {
		if (b1[ i ] != b2[ i ]) {
			printf("Buffers differ at index %u.\n", i);
			exit(-1);
		}
	}

	puts("Comparison confirms match.\n");
	
} /* data_compare() */


/* data_confirm_zeroes()
 *
 * in:     length - buffer length in bytes
 *         buffer - buffer ideally filled with zeroes
 * return: nothing
 *
 * Return if buffer contains zeroes, else exit(-1).
 *
 */

static void data_confirm_zeroes(unsigned char *buffer, unsigned int length) {

	unsigned int i;    /* index into buffer */

	for (i = 0; i < length; i++) {
		if (buffer[ i ]) {
			printf("Buffer has non-zero value at index %u.\n", i);
			exit(-1);
		}
	}

	puts("Examination confirms all-zeroes.\n");
	
} /* data_confirm_zeroes() */


// User level "main"
int tester_main(volatile unsigned long *ioregister) {
	
	init_framework(ioregister, NULL);

	printf("Test: store %u bytes to device, retrieve them, and "
	       "compare.\n\n", DATA_SIZE);
	
	data_init(data, DATA_SIZE);
	puts("Data to write to device:");
	data_print(data, DATA_ROWS, DATA_COLUMNS);

	puts("Writing data...");
	if (DATA_SIZE != write_nand(data, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to write %u bytes to storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		exit(-1);
	}
	puts("Reading data...");
	if (DATA_SIZE != read_nand(dest, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to read %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		exit(-1);
	}

	puts("Data read from device (ideally identical):");
	data_print(dest, DATA_ROWS, DATA_COLUMNS);
	data_compare(data, dest, DATA_SIZE);

	printf("Test: erase device blocks, retrieve erased data, "
	       "and confirm it is zeroed:\n\n");
	puts("Erasing blocks...");
	if (DATA_SIZE != erase_nand(STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to erase %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		exit(-1);
	}
	puts("Reading erased blocks...");
	if (DATA_SIZE != read_nand(dest, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to read %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		exit(-1);
	}

	puts("Data read from device (ideally zeroed):");
	data_print(dest, DATA_ROWS, DATA_COLUMNS);
	data_confirm_zeroes(dest, DATA_SIZE);

	return 0;
}
