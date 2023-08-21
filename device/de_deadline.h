#ifndef _DE_DEADLINE_H_
#define _DE_DEADLINE_H_

void deadline_clear(void);
void deadline_init(void);

bool before_deadline(void);
void set_deadline(timeus_t);

#endif
