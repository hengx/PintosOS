#include <bitmap.h>
#include <stdio.h>
#include <stdint.h>
#include <debug.h>
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "userprog/syscall.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "vm/frame.h"

#define SECTOR_NUM (PGSIZE / BLOCK_SECTOR_SIZE)

/* Swap initial. */
void
swap_init(void)
{
  // Get swap block.
  swap_block = block_get_role(BLOCK_SWAP);

  if(!swap_block)
    return;
  else
    // initial swap_bitmap
    swap_bitmap = bitmap_create(block_size(swap_block) / SECTOR_NUM);
  if(!swap_bitmap)
    return;
  
  //set initial swap_bitmap value
  bitmap_set_all(swap_bitmap, 0);

  //initital swap lock.
  lock_init(&swap_lock);  
}


/* Swap in. */
void
swap_in (struct spt_entry *spte)
{
  size_t i;
  lock_acquire(&swap_lock);

  bitmap_flip(swap_bitmap, spte->swap_sector);

  /* Read page back to memory. */
  for(i = 0; i < SECTOR_NUM; i++) {
    block_read(swap_block, spte->swap_sector * SECTOR_NUM + i,
	       spte->upage + i * BLOCK_SECTOR_SIZE);
  }
  
  lock_release(&swap_lock);

  spte->swap = false;
  spte->loaded = true;
}


/* Swap out. */
void
swap_out (struct spt_entry *spte)
{
  if(!swap_bitmap)
    exit(-1);
  lock_acquire(&swap_lock);

  // Get a free sector
  size_t free_sector = bitmap_scan_and_flip(swap_bitmap, 0, 1, 0);
  size_t i;

  // Record the page to this sector.
  for(i = 0; i < SECTOR_NUM; i++) {
    block_write(swap_block, free_sector * SECTOR_NUM + i,
		spte->upage + i * BLOCK_SECTOR_SIZE);
  }
  
  lock_release(&swap_lock);

  // record swap sector and set "swap" to true.
  spte->swap_sector = free_sector;
  spte->swap = true;
  spte->loaded = false;
}

