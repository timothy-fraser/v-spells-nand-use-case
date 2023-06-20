// Copyright (c) 2022 Provatek, LLC.

#include <sys/types.h>
#include <stdio.h>

#include "device_emu.h"
#include "framework.h"
#include "tester.h"

static struct nand_storage_chip init_storage = {
	.controller = NULL,    /* must be set before test! */
	.nblocks = NUM_BLOCKS,
	.npages_per_block = NUM_PAGES,
	.nbytes_per_page = NUM_BYTES,
	.ref_count = 1
};

static struct nand_controller_chip init_controller = {
	.first_storage = &init_storage,
	.last_storage = &init_storage,
	.nstorage = 1,
	.ref_count = 1,
	.exec_op = NULL
};

static struct nand_device init_device = {
	.device_makemodel = "Dummy device in original DIB",
	.controller = &init_controller,
	.next_device = NULL,
	.ref_count = 2
};


struct nand_device *
st_dib_init(void) {

	init_storage.controller= &init_controller;
	return &init_device;

} /* st_dib_init() */


int
st_dib_test(struct nand_device *dib_old, struct nand_device *dib_new) {
	
	puts("Verifying new DIB...");
	if (verify_dib(dib_new)) {
		puts("Fail - new DIB is not well-formed.");
		return -1;
	}
	
	/* If this driver updated the DIB properly, its new device
	 * should be the first device in the list and the old DIB
	 * should begin with the second device in the list.
	 */
	if (!(dib_new->next_device) ||
	    (dib_new->next_device != dib_old)) {
		puts("Fail - driver failed to preserve original DIB entries.");
		return -1;
	}
	
	puts("Pass - "
		"confirmed DIB well-formed after driver initialization.\n");
	
	return 0;

} /* st_dib_test() */
