// Copyright (c) 2022 Provatek, LLC.

#ifndef DEVICE_EMU_H_
#define DEVICE_EMU_H_

/* IO Register offsets */
#define IOREG_COMMAND 0x02
#define IOREG_ADDRESS 0x01
#define IOREG_DATA    0x00

/* Read commands */
#define C_READ_SETUP   0x01
#define C_READ_EXECUTE 0x02

/* Program commands */
#define C_PROGRAM_SETUP   0x03
#define C_PROGRAM_EXECUTE 0x04

/* Erase commands */
#define C_ERASE_SETUP   0x05
#define C_ERASE_EXECUTE 0x06

/* Extra commands */
#define C_DUMMY 0x07

/* Device Emulator busy/ready */
#define DEVICE_BUSY  1
#define DEVICE_READY 0

/* Device Emulator pins */
#define PN_STATUS 0
#define PN_RESET  1

/* data storage constants */
#define NUM_BLOCKS 256
#define NUM_PAGES  256
#define NUM_BYTES  256

void device_init(volatile unsigned long *in_ioregisters, pid_t child_pid);

#endif
