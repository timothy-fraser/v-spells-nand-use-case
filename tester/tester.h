#ifndef TESTER_H_
#define TESTER_H_

// Copyright (c) 2022 Provatek, LLC.

int st_deterministic(void);
int st_stochastic(long);

struct nand_device *st_dib_init(void);
int st_dib_test(struct nand_device *, struct nand_device *);

#endif
