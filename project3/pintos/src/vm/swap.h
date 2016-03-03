#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "vm/page.h"

struct block *swap_block;
struct bitmap *swap_bitmap;
struct lock swap_lock;

void swap_init(void);
void swap_in (struct spt_entry *spte);
void swap_out (struct spt_entry *spte);

#endif /* vm/swap.h */
