// Copyright (c) 2022 Provatek, LLC.

/*
 * Routines to emulate device cursor.
 */

#include <sys/types.h>
#include <stdbool.h>

#include "device_emu.h"
#include "de_cursor.h"

/* cursor */
unsigned int cursor = 0;


/*
 * increment_cursor()
 *
 * in:  cursor - the current value of the cursor
 *      remain - wrap cursor to remain in current page or not
 * out: cursor - the cursor incremented by one
 * return: nothing
 *
 * Increments the cursor by one. If remain is true, then the cursor is wrapped
 * to stay within the same page. Otherwise the cursor is only wrapped back to
 * zero when it is incremented past the end of storage.
 */

void
increment_cursor(bool remain) {
	
	if (remain) {
		unsigned int bytes = cursor & CURSOR_BYTE_MASK;
		if (bytes + 1 < NUM_BYTES)
			cursor += 1;
		else
			cursor = cursor & ~CURSOR_BYTE_MASK;
			
	} else {
		cursor += 1;

		// wrap cursor to remain in storage
		if (cursor >= (NUM_BLOCKS * NUM_PAGES * NUM_BYTES))
			cursor = 0;
	}
}

/*
 * increment_page()
 *
 * in:  cursor - the current value of the cursor
 * out: cursor - the cursor incremented to the beginning of the next page
 * return: nothing
 *
 * Increments the cursor to the next page. Increments the block to the next
 * block if the current page is the last page in a block. Wraps the cursor
 * back to zero if this is the last page in storage.
 */

void
increment_page(void) {
	
	unsigned int page = cursor & CURSOR_PAGE_MASK;
	page = page >> CURSOR_PAGE_SHIFT;

	unsigned int block = cursor & CURSOR_BLOCK_MASK;
	block = block >> CURSOR_BLOCK_SHIFT;

        page += 1;

        if (page >= NUM_PAGES) {
		page = 0;
		block += 1;
		if (block >= NUM_BLOCKS)
			block = 0;
	}

        cursor = 0;
	cursor = cursor | (block << CURSOR_BLOCK_SHIFT);
	cursor = cursor | (page << CURSOR_PAGE_SHIFT);
}

/*
 * increment_block()
 *
 * in:  cursor - the current value of the cursor
 * out: cursor - the cursor incremented to the beginning of the next block
 * return: nothing
 *
 * Increments the cursor to the next block. Wraps the cursor back to zero if
 * this is the last block in storage.
 */

void
increment_block(void) {
	
	unsigned int block = cursor & CURSOR_BLOCK_MASK;
	block = block >> CURSOR_BLOCK_SHIFT;

	block += 1;

	if (block >= NUM_BLOCKS) {
		block = 0;
	}

	cursor = 0;
	cursor = cursor | (block << CURSOR_BLOCK_SHIFT);
}

/*
 * set_cursor_byte()
 *
 * in:  cursor - the current value of the cursor
 *      value - the value to set the cursor byte to
 *      shift - the byte in the cursor to set (left shift)
 * out: cursor - the cursor with the specified byte set
 * return: nothing
 *
 * Sets the specified byte in the cursor to value.
 */

void
set_cursor_byte(unsigned int value, unsigned int shift) {       
	cursor = cursor & ~(0xFF << shift); // clear the byte
	cursor = cursor | (value << shift);
}
