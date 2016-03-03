#include <stdint.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

uint8_t *frame_evict(enum palloc_flags flag);


/* Set a frame to a supplemental page. */
uint8_t *
palloc_get_frame(enum palloc_flags flag, struct spt_entry *spte)
{
  uint8_t *frame = palloc_get_page (flag);

  // Get a free frame, connect it with a supplemental page
  // add it to the frame table
  if(frame) {
    add_to_frame_table(frame, spte);
  }
  else {
    // No free frame now, evict one to get a free frame
    if(!frame){
      frame = frame_evict(flag);
    }
    if(!frame) {
      PANIC("No free frame.");
    }
    add_to_frame_table(frame, spte);
  }

  return frame;
}


/* Set a frame and add it to the frame table. */
void
add_to_frame_table(uint8_t *frame, struct spt_entry *spte)
{
  struct frame_entry *fe = malloc(sizeof(struct frame_entry));

  // record frame address and the user page
  fe->frame = frame;
  fe->spte = spte;
  fe->owner = thread_current();

  spte->frame = frame;
  
  lock_acquire(&frame_lock);
  list_push_back(&frame_table, &fe->elem);
  lock_release(&frame_lock);
}


/* Free a frame with a specific address. */
void
free_frame(uint8_t *frame)
{
  lock_acquire(&frame_lock);
  struct list_elem *e = list_begin(&frame_table);
  for(; e != list_end(&frame_table); e = list_next(e)) {
    struct frame_entry *fe = list_entry(e, struct frame_entry, elem);

    // find this frame, free it.
    if(fe->frame == frame)
    {
        list_remove(e);
        palloc_free_page(frame);
        free(fe);
        break;
    }
  }
  lock_release(&frame_lock);
}


/* Evict a frame. */
    uint8_t *
frame_evict(enum palloc_flags flag)
{
    lock_acquire(&frame_lock);
    struct list_elem *e = list_begin(&frame_table);

    // check all the frame in frame table
    while(true){
        struct frame_entry *fe = list_entry(e, struct frame_entry, elem);

        struct thread *t = fe->owner;
        if(!fe->spte->pinned) {
            if(pagedir_is_accessed(t->pagedir, fe->spte->upage))
            {
                pagedir_set_accessed(t->pagedir, fe->spte->upage, false);
            }
            else
            {
                // if it is changed
                if(pagedir_is_dirty(t->pagedir, fe->spte->upage)) {
                    // memory mapped files, write it back to the file
                    if(fe->spte->mmap)
                        file_write_at(fe->spte->file,
                                fe->frame,
                                fe->spte->read_bytes,
                                fe->spte->ofs);
                    // user pages, swap it out
                    else {
                        swap_out (fe->spte);
                    }
                }
                // free a frame
                fe->spte->loaded = false;
                list_remove(&fe->elem);
                pagedir_clear_page(t->pagedir, fe->spte->upage);
                palloc_free_page(fe->frame);
                free(fe);

                lock_release(&frame_lock);
                // get a free frame, return it.
                return palloc_get_page(flag);
            }
        }

        // check the next frame
        e = list_next(e);
        if(e == list_end(&frame_table))
            e = list_begin(&frame_table);
    }  
}
