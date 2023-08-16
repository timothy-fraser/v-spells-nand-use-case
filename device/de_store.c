/* Device emulator storage module encapsulting cursor, cache, and data store.
 *
 * Copyright (c) 2022 Provatek, LLC.
 * Copyright (c) 2023 Timothy Jon Fraser Consulting LLC.
 *
 */

#include <sys/types.h>
#include <stdbool.h>
#include <string.h>

#include "device_emu.h"
#include "de_store.h"

/* cursor */
static unsigned int cursor = 0;

/* array to store the simulated flash storage */
static unsigned char data_store[NUM_BLOCKS * NUM_PAGES * NUM_BYTES];

/* array to store the cache */
static unsigned char cache[NUM_BYTES];


/* store_clear_cache()
 *
 * in:     nothing
 * out:    cache cleared via side effect
 * return: nothing
 *
 * Clears the cache to all-zeroes.
 *
 */

void
store_clear_cache(void) {
	memset(cache, 0, sizeof(cache));
}


/* store_clear_cursor()
 *
 * in:     nothing
 * out:    cursor cleared via side effect
 * return: nothing
 *
 * Clears cursor to 0.
 *
 */

void
store_clear_cursor(void) {

	cursor = 0;

} /* store_clear_cursor() */


/* store_init()
 *
 * in:     nothing
 * out:    cache, cursor, and data store all cleared via side effect
 * return: nothing
 *
 * Call this function on startup to produce a cleared all-zeroes
 * cache, cursor, and data store.
 *
 */

void
store_init(void) {

	store_clear_cache();
	store_clear_cursor();
	memset(data_store, 0, sizeof(data_store));

} /* store_init() */


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


/* store_copy_page_to_cache()
 *
 * in:     cursor - indicates page in data store to read into cache
 * out:    cache updated via side effect
 * return: nothing
 *
 * Copies the full page indicated by cursor from data store into cache.
 * Copy is always page-aligned; the entire page containing the byte
 * that the cursor indicates is copied.
 *
 */

void
store_copy_page_to_cache(void) {
	memcpy(cache, &data_store[cursor & ~CURSOR_BYTE_MASK], NUM_BYTES);
}


/* store_copy_page_from_cache()
 *
 * in:     cursor - indicates page in data store to receive data
 * out:    data_store modified by side-effect
 * return: nothing
 *
 * Copies a page of data from cache to the page in the data store
 * indicated by cursor.  The copy is always an entire page, and is
 * always page aligned.  The entire data store page containing the
 * byte indicated by the cursor gets updated.
 *
 */

void
store_copy_page_from_cache(void) {
	
	for (int i=0; i < NUM_BYTES; i++) {
		int idx = (cursor & ~CURSOR_BYTE_MASK);
		idx += i;
		data_store[idx] = cache[i];
	}

} /* store_copy_from_cache() */


/* store_get_cache_byte()
 *
 * in:     cursor - indicates which byte to get from cache
 *         cache  - holds byte to get
 * out:    nothing
 * return: byte read from cache
 *
 * Reads the byte indicated by cursor from cache.
 *
 */

unsigned char
store_get_cache_byte(void) {
	return cache[cursor & CURSOR_BYTE_MASK];
}


/* store_set_cache_byte()
 *
 * in:     byte - byte value to store in cache
 *         cursor - cache location to store in
 * out:    cache updated via side effect
 * return: nothing
 *
 * Stores byte in cache location indicated by cursor.
 *
 */

void
store_set_cache_byte(unsigned char byte) {
	cache[cursor & CURSOR_BYTE_MASK] = byte;
}


/* store_erase_block()
 *
 * in:     cursor - indicates block to erase
 * out:    data store updated via side effect
 * return: nothing
 *
 * Erases the data store block indicated by cursor.  Erase is always
 * block aligned.  The entire block that contains the byte indicated
 * by cursor gets erased.
 *
 */

void
store_erase_block(void) {
	memset(&data_store[(cursor & CURSOR_BLOCK_MASK)], 0,
		NUM_PAGES*NUM_BYTES);
}
