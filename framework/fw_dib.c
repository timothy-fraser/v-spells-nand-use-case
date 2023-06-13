// Copyright (c) 2022 Provatek, LLC.

#include <stdio.h>

#include "framework.h"


/* verify_storage()
 *
 * in:     p_storage - the DIB storage node to verify
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     DIB storage node is well-formed
 *          -1     DIB storage node is not well-formed
 *
 * Examines DIB storage node and indicates whether or not it is well-formed.
 *
 */

static int
verify_storage(struct nand_controller_chip *p_controller,
	struct nand_storage_chip *p_storage) {

	/* The first and last storage chips in the list point back to
	 * the controller.  The rest have NULLs in their controller
	 * field.
	 */
	if ((p_controller->first_storage == p_storage) ||
	    (p_controller->last_storage == p_storage)) {
		if (p_storage->controller != p_controller) {
			puts("DIB: first and last storage chip do not "
				"point to controller chip.");
			return -1;
		}
	} else {
		if (p_storage->controller != NULL) {
			puts("DIB: middle controller chips have "
				"non-NULL controller fields.");
			return -1;
		}
	}

	/* Ref count field must be 1. */
	if (p_storage->ref_count != 1) {
		puts("DIB: storage chip has reference count != 1.");
		return -1;
	}

	/* Well-formed storage node! */
	return 0;

} /* verify_storage() */


/* verify_controller()
 *
 * in:     p_controller - the DIB controller to verify
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     DIB controller is well-formed
 *          -1     DIB controller is not well-formed
 *
 * Examines DIB controller and indicates whether or not it is well-formed.
 *
 */

static int
verify_controller(struct nand_controller_chip *p_controller) {

	unsigned int sc_count = 0;            /* count of controller chips */
	struct nand_storage_chip *p_sc;     /* iterates thru storage chips */
	struct nand_storage_chip *p_last;     /* last storage chip in list */

	/* Controller chip must have at least one storage chip. */
	if (!p_controller->first_storage) {
		puts("DIB:  controller chip has no storage chips.");
		return -1;
	}
	
	/* Examine each of the controller's storage chips. */
	for (p_sc = p_controller->first_storage; p_sc != NULL;
		p_sc = p_sc->next_storage) {

		/* Controller can have no more than max number of
		 * storage chips.
		 */
		if (++sc_count > MAX_STORAGE_CHIPS) {
			puts("DIB:  controller chip has too many "
				"storage chips.");
			return -1;
		}

		/* Each storage chip must be well-formed. */
		if (verify_storage(p_controller, p_sc)) return -1;

		/* remember last storage chip we've seen. */
		p_last = p_sc;
	}
	
	/* The controller must be linked to the last storage chip in
	 * the list.
	 */
	if (p_controller->last_storage != p_last) {
		puts("DIB: controller chip not linked to last "
			"storage chip.");
		return -1;
	}

	/* The controller's reference count must equal its count of
	 * storage nodes, and its storage node count must be accurate.
	 */
	if ((p_controller->ref_count != p_controller->nstorage) ||
	    (p_controller->nstorage != sc_count)) {
		puts("DIB: controller reference count is incorrect.");
		return -1;
	}
	
	/* Well-formed controller node! */
	return 0;
	
} /* verify_controller() */


/* verify_device()
 *
 * in:     p_device - the DIB device to verify
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     DIB device is well-formed
 *          -1     DIB device is not well-formed
 *
 * Examines DIB device and indicates whether or not it is well-formed.
 *
 */

static int
verify_device(struct nand_device *p_device) {

	/* Each device must have a controller. */
	if (!p_device->controller) {
		puts("DIB: device has no controller chip.");
		return -1;
	}

	/* The controller must be well-formed. */
	if (verify_controller(p_device->controller)) return -1;
	
	/* Device's reference count must be one greater than its
	 * controller's reference count.
	 */
	if (p_device->ref_count != p_device->controller->ref_count + 1) {
		puts("DIB: device reference count is incorrect.");
		return -1;
	}

	/* Well-formed device! */
	return 0;
	
} /* verify_device() */


/* verify_dib()
 *
 * in:     dib - the DIB to verify
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     DIB is well-formed
 *          -1     DIB is not well-formed
 *
 * Examines DIB and indicates whether or not it is well-formed.
 *
 */

int
verify_dib(struct nand_device *dib) {

	unsigned int device_count = 0; /* count of devices in DIB */
	struct nand_device *p_device;  /* iterates through DIB devices */

	/* Verify each device in the DIB.  Note that a NULL DIB is an
	 * empty DIB, and this is still considered well-formed.
	 */
	for(p_device = dib; p_device != NULL;
		p_device = p_device->next_device) {

		printf("Verifying: %s.\n", p_device->device_makemodel);

		/* DIB must have no more than max number of devices. */
		if (++device_count > MAX_NAND_DEVICES) return -1;

		if (verify_device(p_device)) return -1;  /* malformed DIB */
	}
	
	return 0;  /* well-formed DIB */

} /* verify_dib() */
