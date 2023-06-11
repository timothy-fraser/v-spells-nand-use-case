
#ifndef _DE_DEADLINE_H_
#define _DE_DEADLINE_H_

/* Durations (microseconds) */
#define READ_PAGE_DURATION   100
#define WRITE_PAGE_DURATION  600
#define ERASE_BLOCK_DURATION 2000

bool before_deadline(void);
void set_deadline(int);

#endif
