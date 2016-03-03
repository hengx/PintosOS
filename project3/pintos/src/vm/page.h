#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <stdint.h>
#include "filesys/file.h"

struct spt_entry {
  void *upage;
  uint8_t *frame;       // frame entry
  struct file *file;    // file
  off_t ofs;            // file offset
  bool writable;        // file can be writable or not
  uint32_t read_bytes;  // read bytes
  uint32_t zero_bytes;  // zero bytes
  bool loaded;          // page is loaded or not
  uint32_t swap_sector; // if pageis swapped, point out the swap sector
  bool swap;            // page is swapped or not
  bool mmap;            // it is a memory mapped file or not
  int mapid;            // if it is a meory mapped file, point out the map id
  bool pinned;          // avoid other process to access when page is using
  
  struct hash_elem elem;
};

struct mmap_entry {
  struct spt_entry *spte;    // point out the related supplement page table
  int mapid;                 // map id
  struct list_elem elem;
};

bool create_page_table (struct file *, off_t, uint8_t *,
			uint32_t, uint32_t, bool);
struct spt_entry* get_spte(void *);
bool load_page(struct spt_entry *);
unsigned page_hash_func(const struct hash_elem *, void *);
bool page_less_func (const struct hash_elem *, const struct hash_elem *, void *);
bool grow_stack(void *fault_addr);
bool create_mmap_page_table(struct file *file, off_t ofs, uint8_t *upage,
			    uint32_t read_bytes, uint32_t zero_bytes);
void page_action_func(const struct hash_elem *e, void *aux);
  
#endif /* vm/page.h */
