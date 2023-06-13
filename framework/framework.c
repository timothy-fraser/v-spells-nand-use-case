// Copyright (c) 2022 Provatek, LLC.

#include <sys/types.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <unistd.h>

#include "fw_jumptable.h"
#include "fw_execop.h"
#include "framework.h"
#include "driver.h"


struct nand_driver driver;

struct nand_device *
init_framework(volatile unsigned long *ioregister,
	struct nand_device *old_dib) {

	struct nand_device *new_dib;  /* DIB possibly updated by driver */
	
	new_dib = init_nand_driver(ioregister, old_dib);
	driver = get_driver();

	/* Initiate a trace.  Parent is the tracer, child is the tracee. */
	ptrace(0, 0, NULL, NULL);

	/* Child pauses itself so that parent can set up watchpoints. */
	kill(getpid(), 5);

	return new_dib;
}


int
write_nand(unsigned char *buffer, unsigned int offset, unsigned int size) {
	
	if (driver.type == NAND_JUMP_TABLE)
	{
		return jt_write(buffer, offset, size);
	}
	else if (driver.type == NAND_EXEC_OP)
	{
		return exec_write(buffer, offset, size);
	}
	return -1;
}


int
read_nand(unsigned char *buffer, unsigned int offset, unsigned int size) {
	
	if (driver.type == NAND_JUMP_TABLE)
	{
		return jt_read(buffer, offset, size);
	}
	else if (driver.type == NAND_EXEC_OP)
	{
		return exec_read(buffer, offset, size);
	}

	return -1;
}


int
erase_nand(unsigned int offset, unsigned int size) {
	
	if (driver.type == NAND_JUMP_TABLE)
	{
		return jt_erase(offset, size);
	}
	else if (driver.type == NAND_EXEC_OP)
	{
		return exec_erase(offset, size);
	}

	return -1;
}

