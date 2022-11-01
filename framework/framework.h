// Copyright (c) 2022 Provatek, LLC.

#ifndef FRAMEWORK_H_
#define FRAMEWORK_H_

#define MAX_NAND_DEVICES 64
#define MAX_STORAGE_CHIPS 8

/* The tracer can use these macros to determine if the tracee trapped
 * on the breakpoint in gpio_set() or the breakpoint in gpio_get().
 * Typically a tracer (debugger) would know the precise address of the
 * breakpoints it has set in the tracee and when responding to a trap
 * would simply retrieve the tracee's instruciton pointer value
 * (register RIP) and look that value up in a list of the addresses of
 * all breakpoints.  In our situation, we're including the breakpoints
 * in the gpio_get/set() function code itself rather than having the
 * tracer set them at runtime.  We don't know the address of the two
 * breakpoint instructions themselves, but we do know which functions
 * they are in.  We know the addresses of all the functions in this .c
 * file and can tell when the tracee's instruction pointer is within a
 * given function.  The following predicates test whether an
 * instruction pointer value lies within a given function.  The
 * predicates identify the last instruction of a given function by
 * counting back from the next function in the program text, so if you
 * add new functions to this file or re-arrange old ones you may need
 * to tweak these predicates.
 */

extern int tester_main();

/* Return true iff function f1 <= address a < function 2. */
#define RIP_IN_FUNCTION(a, f1, f2) \
        (((void *)a >= (void *)f1) && ((void *)a < (void *)f2))
/* Return true if address a is in named function. */
#define RIP_IN_GPIO_SET(a) RIP_IN_FUNCTION(a, &gpio_set, &gpio_get)
#define RIP_IN_GPIO_GET(a) RIP_IN_FUNCTION(a, &gpio_get, &tester_main)

enum nand_op_instr_type {
	NAND_OP_CMD_INSTR,
	NAND_OP_ADDR_INSTR,
	NAND_OP_DATA_IN_INSTR,
	NAND_OP_DATA_OUT_INSTR,
	NAND_OP_WAITRDY_INSTR,
};

struct nand_op_cmd_instr {
	unsigned char opcode;
};

struct nand_op_addr_instr {
	unsigned int naddrs;
	const unsigned char* addrs; // [0]BLOCK [1]PAGE [2]BYTE
};

struct nand_op_data_instr {
	unsigned int len;
	union {
		const void* in;
		void* out;
	} buf;
};

struct nand_op_waitrdy_instr {
	unsigned int timeout_ms;
};

struct nand_op_instr {
	enum nand_op_instr_type type;
	union {
		struct nand_op_cmd_instr cmd;
		struct nand_op_addr_instr addr;
		struct nand_op_data_instr data;
		struct nand_op_waitrdy_instr waitrdy;
	} ctx;
};

struct nand_operation {
	const struct nand_op_instr* instrs;
	unsigned int ninstrs;
};

struct nand_storage_chip
{
	unsigned int nblocks;
	unsigned int npages_per_block;
	unsigned int nbytes_per_page;
	struct nand_storage_chip* next_storage;
	struct nand_controller_chip* controller;
	unsigned int ref_count;
};

struct nand_controller_chip 
{
	unsigned int nstorage;
	struct nand_storage_chip* first_storage;
	struct nand_storage_chip* last_storage;
	int (*exec_op)(struct nand_operation *commands);
	unsigned int ref_count;
};

struct nand_device
{
	struct nand_controller_chip* controller;
	struct nand_device* next_device;
	const char* device_makemodel;
	unsigned int ref_count;
};

// DRIVER INTERFACE

struct nand_jump_table
{
	void (*set_register)(unsigned char offset, unsigned char value);
	int (*read_buffer)(unsigned char *buffer, unsigned int length);
	int (*write_buffer)(unsigned char *buffer, unsigned int length);
	int (*wait_ready)(unsigned int interval_us);
};

enum nand_driver_type {
	NAND_JUMP_TABLE,
	NAND_EXEC_OP,
};

struct nand_driver
{
	enum nand_driver_type type;
	union {
		struct nand_jump_table jump_table;
		int (*exec_op)(struct nand_operation *commands);
	} operation;
};

void gpio_set(unsigned int pin, unsigned int value);
unsigned int gpio_get(unsigned int pin);

// USER/TESTER INTERFACE

struct nand_device *init_framework(volatile unsigned long *ioregister,
	struct nand_device *old_dib);
int write_nand(unsigned char *buffer, unsigned int offset, unsigned int size);
int read_nand(unsigned char *buffer, unsigned int offset, unsigned int size);
int erase_nand(unsigned int offset, unsigned int size);

int verify_dib(struct nand_device *nand_device);

#endif
