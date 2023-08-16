#ifndef _DE_PARSER_H_
#define _DE_PARSER_H_

void parser_reset(void);
void parser_init(volatile unsigned long *);
void handle_watchpoint_ioregisters(pid_t, struct user_regs_struct *);

#endif
