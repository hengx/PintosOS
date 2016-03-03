#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

void syscall_init (void);
void exit (int);
//bool is_valid_ptr(const void *user_ptr, struct intr_frame *f);
//bool is_valid_ptr(const void *, struct intr_frame *);

#endif /* userprog/syscall.h */
