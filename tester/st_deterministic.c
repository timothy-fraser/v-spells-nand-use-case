// Copyright (c) 2022 Provatek, LLC.

#include <stdio.h>

#include "framework.h"
#include "st_data.h"
#include "tester.h"

#define STORAGE_ADDR 0

/* These constants define the amount of data we'll be storing and
 * retrievig from the device in terms of rows and columns of
 * characters we can neatly print to the console.
 */
#define DATA_ROWS    5
#define DATA_COLUMNS 60
#define DATA_SIZE    (DATA_ROWS * DATA_COLUMNS)


static unsigned char data[DATA_SIZE];
static unsigned char dest[DATA_SIZE];


/* st_deterministic()
 *
 * in:     nothing
 * out:    nothing
 * return: 0 if all tests passed, else -1.
 *
 * Run a deterministic system test on the framework-driver-device system.
 *
 */

int
st_deterministic(void) {

	unsigned int index;   /* index returned by data compare fxns */
	
	printf("Test: store %u bytes to device, retrieve them, and "
	       "compare.\n\n", DATA_SIZE);
	
	data_init(data, DATA_SIZE);
	puts("Data to write to device:");
	data_print(data, DATA_ROWS, DATA_COLUMNS);

	puts("Writing data...");
	if (write_nand(data, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to write %u bytes to storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		return -1;
	}
	puts("Reading data...");
	if (read_nand(dest, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to read %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		return -1;
	}		

	puts("Data read from device (ideally identical):");
	data_print(dest, DATA_ROWS, DATA_COLUMNS);
	if (DATA_SIZE == (index = data_compare(data, dest, DATA_SIZE))) {
		puts("Comparison confirms match.\n");
	} else {
		printf("Buffers differ at index %u.\n", index);
		return -1;
	}

	printf("Test: erase device blocks, retrieve erased data, "
	       "and confirm it is zeroed:\n\n");

	puts("Erasing blocks...");
	if (erase_nand(STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to erase %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		return -1;
	}
	puts("Reading erased blocks...");
	if (read_nand(dest, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to read %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		return -1;
	}

	puts("Data read from device (ideally zeroed):");
	data_print(dest, DATA_ROWS, DATA_COLUMNS);
	if (DATA_SIZE == (index = data_confirm_zeroes(dest, DATA_SIZE))) {
		puts("Examination confirms all-zeroes.\n");
	} else {
		printf("Buffer has non-zero value at index %u.\n", index);
		return -1;
	}

	return 0;  /* All tests passed! */
	
} /* st_deterministic() */
