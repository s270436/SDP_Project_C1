#ifndef _FREE_BITMAP_H_
#define _FREE_BITMAP_H_

#include "opt-projectc1.h"

#if OPT_PROJECTC1
void bitmap_init(unsigned int length);

void bitmap_set_free(unsigned int i);

void bitmap_set_busy(unsigned int i);

int bitmap_find_first_free(unsigned int * first_free);

void bitmap_destroy_mem(void);
#endif

#endif