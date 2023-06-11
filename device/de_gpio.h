
#ifndef _DE_GPIO_H_
#define _DE_GPIO_H_

void handle_breakpoint_gpio_set(struct user_regs_struct *);
void handle_breakpoint_gpio_get(pid_t, struct user_regs_struct *);

#endif
