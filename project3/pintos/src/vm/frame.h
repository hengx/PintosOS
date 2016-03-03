#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <list.h>

struct list frame_table;
struct lock frame_lock;

struct frame_entry {
  uint8_t *frame;               // frame address
  struct spt_entry *spte;       // page entry
  struct thread *owner;         // thread id
  struct list_elem elem;
};

uint8_t *palloc_get_frame(enum palloc_flags, struct spt_entry *spte);
void add_to_frame_table(uint8_t *frame, struct spt_entry *spte);
void free_frame(uint8_t *frame);

#endif /* vm/frame.h */
