#ifndef _FW_JUMPTABLE_H_
#define _FW_JUMPTABLE_H_

int jt_write(unsigned char *, unsigned int, unsigned int);
int jt_read(unsigned char *, unsigned int, unsigned int);
int jt_erase(unsigned int, unsigned int);


#endif
