#include "vm/page.h"
#include <stdbool.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/swap.h"

bool  load_from_swap(struct spt_entry *spte);


/* Add supplement page to page table.*/
bool
create_page_table (struct file *file, off_t ofs, uint8_t *upage,
		   uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  if(!spte)
    return false;
  spte->file = file;
  spte->ofs = ofs;
  // round down the page address.
  spte->upage = pg_round_down(upage);
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->loaded = false;
  spte->swap = false;
  spte->mmap = false;
  spte->pinned = false;

  struct thread *t = thread_current();

  // Insert into hash table
  return (hash_insert(&t->spt, &spte->elem) == NULL);
}


/* Add memory mapped files to supplemental page table. */
bool
create_mmap_page_table(struct file *file, off_t ofs, uint8_t *upage,
		   uint32_t read_bytes, uint32_t zero_bytes)
{
  if(get_spte(pg_round_down(upage))){
    return false;
  }
  
  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  struct thread *t = thread_current();
  if(!spte)
    return false;
  spte->file = file;
  spte->ofs = ofs;
  // round down the page address.
  spte->upage = pg_round_down(upage);
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = true;
  spte->loaded = false;
  spte->swap = false;
  spte->mmap = true;
  spte->pinned = false;

  struct mmap_entry *mme = malloc(sizeof(struct mmap_entry));
  mme->mapid = t->mapid;
  mme->spte = spte;

  list_push_back(&t->mmap_list, &mme->elem);
  return (hash_insert(&t->spt, &spte->elem) == NULL);
}


/* Get supplemental page table by upage. */
struct spt_entry*
get_spte(void *upage)
{
  struct spt_entry spte;
  // round down the page address.
  spte.upage = pg_round_down(upage);
  
  struct thread *t = thread_current();

  // Find the page table.
  struct hash_elem *e = hash_find(&t->spt, &spte.elem);

  if(!e) {
    return NULL;
  }

  return hash_entry(e, struct spt_entry, elem);
}


/* Load page to memory. */
bool
load_page(struct spt_entry *spte)
{
   // this page is already in the memory.
   if(spte->loaded)
     return true;
   
   //page is swapped out, load it from swap.
   if(spte->swap){
     return load_from_swap(spte);
   }
  
   enum palloc_flags flags = PAL_USER;
   if(spte->read_bytes == 0)
     flags |= PAL_ZERO;
   
   /* Get a page of memory. */
   uint8_t *kpage = palloc_get_frame(flags, spte);
   if (kpage == NULL)
     return false;

   /* Load this page. */
   if (file_read_at (spte->file, kpage,
		     spte->read_bytes, spte->ofs)
       != (int) spte->read_bytes)
   {
     free_frame(kpage);
     return false; 
   }
   memset (kpage + spte->read_bytes, 0, spte->zero_bytes);

   /* Add the page to the process's address space. */
   if (!install_page (spte->upage, kpage, spte->writable)) 
   {
     free_frame(kpage);
     return false; 
   }

   spte->loaded = true;
   
   return true;
}


/* Load page from the swap. */
bool 
load_from_swap(struct spt_entry *spte)
{
   /* Get a page of memory. */
   uint8_t *kpage = palloc_get_frame(PAL_USER, spte);
   if (!kpage)
     return false;
   
   if (!install_page (spte->upage, kpage, spte->writable)) 
   {
     free_frame(kpage);
     return false; 
   }

   // Call swap_in to load page from swap to memory.
   swap_in(spte);
   return true;
}

unsigned
page_hash_func(const struct hash_elem *e, void *aux UNUSED)
{
  struct spt_entry *spte = hash_entry(e, struct spt_entry, elem);
  return hash_int((int)spte->upage);
}

bool
page_less_func (const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux UNUSED)
{
  struct spt_entry *sptea = hash_entry(a, struct spt_entry, elem);
  struct spt_entry *spteb = hash_entry(b, struct spt_entry, elem);

  return sptea->upage < spteb->upage;
}

void
page_action_func(const struct hash_elem *e, void *aux UNUSED)
{
  struct spt_entry *spte = hash_entry(e, struct spt_entry, elem);
  if(spte->loaded) {
    free_frame(spte->frame);
    pagedir_clear_page(thread_current()->pagedir, spte->upage);
  }
  free(spte);
}


/* Stack growth*/
bool
grow_stack(void *fault_addr)
{
  if((size_t)(PHYS_BASE - pg_round_down(fault_addr)) > (1 << 23))
    return false;

  struct spt_entry *spte = malloc(sizeof(struct spt_entry));
  if(!spte)
    return false;
  
  spte->upage = pg_round_down(fault_addr);
  spte->writable = true;
  spte->loaded = true;
  spte->swap = true;
  spte->pinned = true;
  
  /* Get a page of memory. */
  uint8_t *frame = palloc_get_frame(PAL_USER, spte);

  if(!frame) {
    free(frame);
    return false;
  }

  // grow stack
  if (!install_page (spte->upage, frame, spte->writable)) 
   {
     free_frame(frame);
     free(spte);
     return false; 
   }

  
  spte->pinned = false;
  // add it to the supplemental page table
  struct thread *t = thread_current();
  return (hash_insert(&t->spt, &spte->elem) == NULL);
}

