// Copyright (c) 2022 Provatek, LLC.

#include <stdio.h>

#include "framework.h"
#include "st_data.h"
#include "tester.h"

#define STORAGE_ADDR 0

#define DATA_SIZE    300   /* how many bytes to write to/read from device */

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
	data_print(data, DATA_SIZE);

	puts("Writing data...");
	fflush(stdout);
	if (write_nand(data, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to write %u bytes to storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		return -1;
	}
	puts("Reading data...");
	fflush(stdout);
	if (read_nand(dest, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to read %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		return -1;
	}		

	puts("Data read from device (ideally identical):");
	data_print(dest, DATA_SIZE);
	if (DATA_SIZE == (index = data_compare(data, dest, DATA_SIZE))) {
		puts("\nPass - comparison confirms match.\n");
	} else {
		printf("\nFail - buffers differ at index %u.\n", index);
		return -1;
	}

	printf("Test: erase device blocks, retrieve erased data, "
	       "and confirm it is zeroed:\n\n");

	puts("Erasing blocks...");
	fflush(stdout);
	if (erase_nand(STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to erase %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		return -1;
	}
	puts("Reading erased blocks...");
	fflush(stdout);
	if (read_nand(dest, STORAGE_ADDR, DATA_SIZE)) {
		printf("Failed to read %u bytes from storage address %u.\n",
		       DATA_SIZE, STORAGE_ADDR);
		return -1;
	}

	puts("Data read from device (ideally zeroed):");
	data_print(dest, DATA_SIZE);
	if (DATA_SIZE == (index = data_confirm_zeroes(dest, DATA_SIZE))) {
		puts("\nPass - examination confirms all-zeroes.\n");
	} else {
		printf("\nFail - buffer has non-zero value at index %u.\n",
			index);
		return -1;
	}

	return 0;  /* All tests passed! */
	
} /* st_deterministic() */
