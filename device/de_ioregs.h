
#ifndef _DE_IOREGS_H_
#define _DE_IOREGS_H_

void handle_watchpoint_setup(pid_t);
void update_tracee_cpu_registers(pid_t,	struct user_regs_struct *,
	unsigned int);

#endif
