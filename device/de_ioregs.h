#ifndef _DE_IOREGS_H_
#define _DE_IOREGS_H_

/* IO Register Masks */
#define MASK_COMMAND 0x00FF0000
#define MASK_ADDRESS 0x0000FF00
#define MASK_DATA    0x000000FF

/* bits to shift IO register to obtain value */
#define COMMAND_SHIFT 16
#define ADDRESS_SHIFT 8


void ioregs_init(volatile unsigned long *, pid_t);
void update_tracee_cpu_registers(pid_t,	struct user_regs_struct *,
	unsigned int);

#endif
