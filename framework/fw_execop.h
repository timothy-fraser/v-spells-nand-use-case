#ifndef _FW_EXECOP_H_
#define _FW_EXECOP_H_

int exec_write(const unsigned char *, unsigned int, unsigned int);
int exec_read(unsigned char *, unsigned int, unsigned int);
int exec_erase(unsigned int, unsigned int);

#endif
