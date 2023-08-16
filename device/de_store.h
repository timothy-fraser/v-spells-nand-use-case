
#ifndef _DE_STORE_H_
#define _DE_STORE_H_

/* cursor address masks */
#define CURSOR_BLOCK_MASK 0x00FF0000
#define CURSOR_PAGE_MASK  0x0000FF00
#define CURSOR_BYTE_MASK  0x000000FF

/* cursor bit shifts */
#define CURSOR_BLOCK_SHIFT 16
#define CURSOR_PAGE_SHIFT   8
#define CURSOR_BYTE_SHIFT   0

void store_clear_cache(void);
void store_clear_cursor(void);
void store_init(void);
void increment_cursor(bool);
void increment_page(void);
void increment_block(void);
void set_cursor_byte(unsigned int, unsigned int);
void store_copy_page_to_cache(void);
void store_copy_page_from_cache(void);
unsigned char store_get_cache_byte(void);
void store_set_cache_byte(unsigned char);
void store_erase_block(void);

#endif
