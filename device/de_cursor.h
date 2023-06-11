
#ifndef _DE_CURSOR_H_
#define _DE_CURSOR_H_

/* cursor address masks */
#define CURSOR_BLOCK_MASK 0x00FF0000
#define CURSOR_PAGE_MASK  0x0000FF00
#define CURSOR_BYTE_MASK  0x000000FF

/* cursor bit shifts */
#define CURSOR_BLOCK_SHIFT 16
#define CURSOR_PAGE_SHIFT   8
#define CURSOR_BYTE_SHIFT   0

void increment_cursor(bool);
void increment_page(void);
void increment_block(void);
void set_cursor_byte(unsigned int, unsigned int);

#endif
