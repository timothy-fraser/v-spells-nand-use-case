
#ifndef _DE_DEVICE_H_
#define _DE_DEVICE_H_

/* Device Emulator states */
#define MS_INITIAL_STATE 0x00000000
#define MS_BUG           0x00000001

/* Device Emulator READ states */
#define MS_READ_AWAITING_BLOCK_ADDRESS 0x00000002
#define MS_READ_AWAITING_PAGE_ADDRESS  0x00000003
#define MS_READ_AWAITING_BYTE_ADDRESS  0x00000004
#define MS_READ_AWAITING_EXECUTE       0x00000005
#define MS_READ_PROVIDING_DATA         0x00000006

/* Device Emulator PROGRAM states */
#define MS_PROGRAM_AWAITING_BLOCK_ADDRESS 0x00000007
#define MS_PROGRAM_AWAITING_PAGE_ADDRESS  0x00000008
#define MS_PROGRAM_AWAITING_BYTE_ADDRESS  0x00000009
#define MS_PROGRAM_ACCEPTING_DATA         0x0000000A

/* Device Emulator ERASE states */
#define MS_ERASE_AWAITING_BLOCK_ADDRESS 0x0000000B
#define MS_ERASE_AWAITING_EXECUTE       0x0000000C


void clear_state(void);
void device_init(volatile unsigned long *, pid_t);


#endif
