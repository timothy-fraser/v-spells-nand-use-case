// Copyright (c) 2022 Provatek, LLC.

#include <stdio.h>

#include "st_data.h"

#define COLUMNS 60  /* print data in rows that are this wide */

/* Macros for determining if a number corresponds to a character we
 * think is printable and for mapping numbers to printable characters.
 */
#define IS_PRINTABLE(i) ('a' <= (i) && (i) <= 'z')
#define TO_PRINTABLE(i) ('a' + (i) % ('z' - '`'))


/* data_init()
 *
 * in:     length - buffer length in bytes
 * out:    buffer - will be filled with printable characters
 * return: nothing
 *
 * Fill the indicated buffer with printable characters.
 *
 */

void
data_init(unsigned char *buffer, unsigned int length) {

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

void
data_print(unsigned char *buffer, unsigned int size) {

	unsigned int i;       /* index into buffer */
	unsigned char b;      /* byte to print */
	
	for (i = 0; i < size; i++) {
		if ((i % COLUMNS) == 0) putchar('\n');
		b = buffer[ i ];
		if (IS_PRINTABLE(b)) {
			putchar(b);
		} else {
			putchar('-');
		}
	}
	putchar('\n');
	
} /* data_print() */


/* data_compare()
 *
 * in:     length - length of both buffers in bytes
 *         b1     - buffer to compare
 *         b2     - buffer to compare
 * out:    nothing
 * return: value            condition
 *         ---------------  ---------
 *         0 <= i < length  index of first difference between b1 and b2.
 *         length           b1 and b2 have identical contents.
 *
 * Compare contents of buffers b1, b2.
 *
 */

unsigned int
data_compare(unsigned char *b1, unsigned char *b2, unsigned int length) {

	unsigned int i;    /* index into buffers */

	for (i = 0; i < length; i++) {
		if (b1[ i ] != b2[ i ]) {
			break;   /* return index of first difference */
		}
	}

	return i;     /* buffers identical, return index == length */
	
} /* data_compare() */


/* data_confirm_zeroes()
 *
 * in:     length - buffer length in bytes
 *         buffer - buffer ideally filled with zeroes
 * return: value            condition
 *         ---------------  ---------
 *         0 <= i < length  index of first non-zero in buffer.
 *         length           buffer is all zeroes.
 *
 * Confirm that buffer contains all zeroes.
 *
 */

unsigned int
data_confirm_zeroes(unsigned char *buffer, unsigned int length) {

	int i;    /* index into buffer */

	for (i = 0; i < length; i++) {
		if (buffer[ i ]) {
			break;  /* return index of first non-zero found */
		}
	}

	return i;  /* all zeroes, return index == length */
	
} /* data_confirm_zeroes() */
