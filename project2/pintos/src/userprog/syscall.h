#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

void syscall_init (void);
void exit (int);
bool is_valid_ptr(const void *);

#endif /* userprog/syscall.h */
