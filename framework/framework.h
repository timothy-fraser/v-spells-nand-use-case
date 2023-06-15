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
 * instruction pointer value lies within a given function.  If you
 * modify the functions they reference and change the lengths of their
 * program text, you'll need to update the predicates.
 */

/* Return true iff function start f <= address a < (f + fxn length l) */
#define RIP_IN_FUNCTION(a, f, l)	\
        (((void *)a >= (void *)f) && ((void *)a < ((void *)f + l)))
/* Return true if address a is in named function. */
#define GPIO_SET_LENGTH 0x0E  /* length of gpio_set() program text */
#define GPIO_GET_LENGTH 0x43  /* length of gpio_get() program text */
#define RIP_IN_GPIO_SET(a) RIP_IN_FUNCTION(a, &gpio_set, GPIO_SET_LENGTH)
#define RIP_IN_GPIO_GET(a) RIP_IN_FUNCTION(a, &gpio_get, GPIO_GET_LENGTH)

/* The NAND_OP_ADDR_INSTR instruction has an array of addresses.  The
 * number of cells in the array that contain meaningful data depends
 * on the preceeding NAND_OP_CMD_INSTR instruction.  Data in and out
 * instructions need three addresses: erase block, page, and byte.
 * Erase instructions need only one: block.
 */
#define NAND_INSTR_BLOCK 0  /* index of erase block number */
#define NAND_INSTR_PAGE  1  /* index of page number */
#define NAND_INSTR_BYTE  2  /* index of byte offset */

#define NAND_INSTR_NUM_ADDR_IO    3
#define NAND_INSTR_NUM_ADDR_ERASE 1
#define NAND_INSTR_NUM_ADDR_MAX   NAND_INSTR_NUM_ADDR_IO


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
	unsigned char addrs[ NAND_INSTR_NUM_ADDR_MAX ];
};

struct nand_op_data_in_instr {  /* write to device */
	unsigned int len;
	const unsigned char *buf;
};

struct nand_op_data_out_instr {  /* read from device */
	unsigned int len;
	unsigned char *buf;
};

struct nand_op_waitrdy_instr {
	unsigned int timeout_ms;
};

struct nand_op_instr {
	enum nand_op_instr_type type;
	union {
		struct nand_op_cmd_instr cmd;
		struct nand_op_addr_instr addr;
		struct nand_op_data_in_instr data_in;
		struct nand_op_data_out_instr data_out;
		struct nand_op_waitrdy_instr waitrdy;
	} ctx;
};

struct nand_operation {
	unsigned int ninstrs;
	const struct nand_op_instr* instrs;
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
	void (*read_buffer)(unsigned char *buffer, unsigned int length);
	void (*write_buffer)(unsigned char *buffer, unsigned int length);
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

void gpio_set(unsigned int, unsigned int);
unsigned int gpio_get(unsigned int);

// USER/TESTER INTERFACE

struct nand_device *init_framework(volatile unsigned long *,
	struct nand_device *);
int write_nand(unsigned char *, unsigned int, unsigned int);
int read_nand(unsigned char *, unsigned int, unsigned int);
int erase_nand(unsigned int, unsigned int);

int verify_dib(struct nand_device *);

#endif
