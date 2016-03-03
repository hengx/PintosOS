#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *f UNUSED);
void get_arguments (struct intr_frame *f, int *arg, int n);
int get_kernel_ptr(const void *vaddr);
void halt(void);
void exit (int status); 
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
struct open_file * get_open_file_by_fd(struct thread *t, int descriptor);
void close (int fd);
int write (int fd, const void *buffer, unsigned size);
int read (int fd, void *buffer, unsigned size);
void seek (int file_desc, unsigned position);
unsigned tell(int fd);
int filesize(int fd);
bool is_valid_ptr(const void *user_ptr);
void check_buffer(const void *buffer, unsigned size);


static struct lock file_lock;

void
syscall_init (void) 
{
  lock_init(&file_lock); //init file_lock;
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* Check if the VADDR is a user virtual address. */
  if(!is_user_vaddr(f->esp))
    exit(-1);

  /* switch part and get_arguments() refer to:
     https://github.com/ryantimwilson/Pintos-Project-2/blob
     /master/src/userprog/syscall.c*/
  int arg[4];
  switch(* (int *) f->esp) {
    case SYS_HALT:{
      halt();
      f->eax = 0;
      break;
    }
    case SYS_EXIT: {
      get_arguments(f, &arg[0],1);
      exit(arg[0]);
      f->eax = 0;
      break;
    }
    case SYS_EXEC: {
      get_arguments(f, &arg[0],1);
      /* Mapping user virtual address to kernel address. */
      f->eax = exec((const char*)get_kernel_ptr((const void *) arg[0]));
      break;
    }
    case SYS_WAIT: {
      get_arguments(f, &arg[0],1);
      f->eax = wait(arg[0]);
      break;
    }
    case SYS_CREATE: {
      get_arguments(f, &arg[0],2);
      const char *file =(const char*) get_kernel_ptr((const void*) arg[0]);
      f->eax = create(file, (unsigned) arg[1]);
      break;
    }
    case SYS_REMOVE:{
      get_arguments(f, &arg[0],1);
      const char *file = (const char*)get_kernel_ptr((const void*) arg[0]);
      f->eax = remove(file);
      break;
    }
    case SYS_OPEN: {
      get_arguments(f, &arg[0],1);
      const char *file = (const char*) get_kernel_ptr((const void*) arg[0]);
      f->eax = open(file);
      break;
    }
    case SYS_FILESIZE: {
      get_arguments(f, &arg[0],1); 
      f->eax = filesize(arg[0]);
      break;
    }
    case SYS_READ: {
      get_arguments(f, &arg[0],3);
      check_buffer((const void *)arg[1], (unsigned) arg[2]);
      void *buffer = (void *) get_kernel_ptr((const void*) arg[1]);
      f->eax = read(arg[0],buffer, arg[2]);
      break;
    }
    case SYS_WRITE: {     
      get_arguments(f, &arg[0],3);
      check_buffer((const void *)arg[1], (unsigned) arg[2]);
      const void *buffer = (const void*)get_kernel_ptr((const void*) arg[1]);
      f->eax = write(arg[0], buffer, (unsigned) arg[2]);
      break;
    }
    case SYS_SEEK: {    
      get_arguments(f, &arg[0],2); 
      seek(arg[0], (unsigned) arg[1]);
      break;
    }
    case SYS_TELL: {    
      get_arguments(f, &arg[0], 1); 
      f->eax = tell(arg[0]);
      break;
    }
    case SYS_CLOSE: {      
      get_arguments(f, &arg[0],1);
      close(arg[0]);
      break;
    }
  }
}


/* Get arguments. */
void
get_arguments (struct intr_frame *f, int *arg, int n)
{
  int i = 0;
  for(; i < n; i++) {
    int *ptr = (int *) f->esp + i + 1;
    
    if(!is_valid_ptr((const void*) ptr))
      exit(-1);
    arg[i] = *ptr;
  }
}

/* Check the range of buffer is valid. */
void
check_buffer(const void *buffer, unsigned size) {
  if(!is_user_vaddr(buffer) || !is_user_vaddr(buffer + size))
    exit(-1);
}


/* Mapping user virtual address to kernel address. */
int get_kernel_ptr(const void *vaddr)
{
  if(!is_user_vaddr(vaddr))
    exit(-1);
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if(ptr == NULL)
    exit(-1);
  return (int) ptr;
}

/* Syscall halt. */
void
halt(void) {
  shutdown_power_off();
}


/* Syscall exit. */
void
exit (int status) {
  
  struct thread *curr = thread_current();

  /* Set thread exit status. */
  curr->self_child->status = status;
  curr->self_child->exit = true;
  
  thread_exit ();  
}


/* Syscall exec. */
pid_t
exec (const char *cmd_line) {

  /* Get execute cmd_line and call process_execute to exec it. */
  char *copyfile = (char *)malloc(sizeof(char) *(strlen(cmd_line)+1));
  memcpy(copyfile, cmd_line, strlen(cmd_line)+1);
  pid_t pid = process_execute(copyfile);

  struct thread *t = thread_current();
  struct list_elem *e = list_begin(&t->children);

  /* Wait all the child processes finish their loading. */
  for( ; e != list_end(&t->children); e = list_next(e)) {
    struct child_process *cp = list_entry(e, struct child_process, elem);

    if (cp->loaded == 0) {
      /* Wait for the sema_exec. If the executable file has been loaded, sema_exec will be sema_up, then the process can be continued. */
      sema_down(&cp->sema_exec);
    }

    /* File load failed. */
    if(cp->loaded == -1) {
      return -1;
    }
  }
  
  free(copyfile);
    
  return pid;
}


/* Syscall wait. Wait for a child process pid and retrieves the child's exit status. */
int
wait (pid_t pid) {
  int result = -1;
  if(pid !=- 1) {
    result = process_wait(pid);
  }

  return result;
}

/* creates a file with the given name and size. */
bool
create (const char *file, unsigned initial_size)
{
  return filesys_create(file, initial_size);
}

/*removes the file with the given name.
*/
bool
remove (const char *file)
{
  return filesys_remove(file);
}


/*opens a file with the given name, returning the fd of that file in the
current thread. if the file cannot be opened, returns -1.
*/
int
open (const char *file)
{
  lock_acquire(&file_lock);
  struct file *f = filesys_open(file);
  lock_release(&file_lock);
  struct thread *cur = thread_current();
  struct open_file *holder = (struct open_file *)malloc(sizeof(struct open_file));
  holder->file = f;

  /* Set the fd. Every time it is opened_files length puls 2. Make sure every file has different fd. */
  holder->fd = 2 + list_size(&cur->opened_files);
  
  
  if (f != NULL)
  {
    /* Add file to the list. */
    list_push_back(&cur->opened_files, &holder->elem);
    return holder->fd;
  }
  else    
    return -1;
}


/*iterates through the file_list of the current thread, returning the first
file in the list whose fd = the given int. if there is no such fd, returns
null.
*/
struct open_file *
get_open_file_by_fd(struct thread *t, int descriptor)
{
  struct list_elem *temp = list_begin (&t->opened_files);
  for (; temp != list_end (&t->opened_files); temp = list_next (temp))
  {
    struct open_file *of= list_entry(temp, struct open_file, elem);
    if (of->fd == descriptor)
    {
      return of;
    }
  }
  return NULL;
}

/*finds the first file in the current threads file_list with the given fd
and closes it.
*/
void
close (int fd)
{
  struct thread *cur = thread_current();
  struct open_file *of = get_open_file_by_fd(cur, fd);
  if(of != NULL) {
    file_close(of->file);
    list_remove(&of->elem);
    free(of);
  }
  else
    exit(-1);
}


/* Syscall write. */
int
write (int fd, const void *buffer, unsigned size)
{
  int ret_stat = -1;
  if (fd == STDIN_FILENO)
    ret_stat = -1;
  /* Write to a buffer. */
  else if (fd == STDOUT_FILENO)
  {
    lock_acquire(&file_lock);
    putbuf (buffer, size);
    lock_release(&file_lock);
    ret_stat = size;
  }
  /* Write to a file. */
  else {
    struct thread *t = thread_current();
    struct open_file *of = get_open_file_by_fd(t, fd);
    
    if (of != NULL) {
      lock_acquire(&file_lock);
      ret_stat = file_write (of->file, buffer, size);
      lock_release(&file_lock);
    }
    else
      ret_stat = -1;
  }
  return ret_stat;
}


/* Syscall read. */
int
read (int fd, void * buffer, unsigned size)
{
  int ret_stat = -1;
  struct thread *t = thread_current();
  unsigned int i = 0;
  uint8_t* new_buffer = (uint8_t *) buffer;
  if (fd == STDOUT_FILENO)
    ret_stat = -1;
  /* Read a buffer. */
  else if (fd == STDIN_FILENO)
  {
    lock_acquire(&file_lock);
    for (; i < size; i++) {
      new_buffer[i] = input_getc();
    }
    lock_release(&file_lock);
    return size;
  }
  /* Read a file. */
  else {
    struct open_file *of = get_open_file_by_fd (t, fd);
    if (of != NULL) {
      lock_acquire(&file_lock);
      ret_stat = file_read (of->file, buffer, size);
      lock_release(&file_lock);
    }
  }
  return ret_stat;
}


/* Syscall seek. */
void
seek (int file_desc, unsigned position)
{
  struct thread *t = thread_current();
  struct open_file *of = get_open_file_by_fd (t, file_desc);
  if (of != NULL) {
    lock_acquire(&file_lock);
    file_seek (of->file, position);
    lock_release(&file_lock);
  }
  return;
}

/* Returns the position of the next byte to be written in open file fd.*/
unsigned
tell(int fd)
{
  off_t ret_stat = 0;
  struct thread *t = thread_current();
  struct open_file *of = get_open_file_by_fd (t, fd);
  if (of != NULL){
    lock_acquire(&file_lock);
    ret_stat = file_tell (of->file);
    lock_release(&file_lock);
  }
  return (unsigned) ret_stat;
}


/* Return the size, in byte, of the file open as fd. */
int
filesize(int fd)
{
  int ret_stat = 0;
  struct thread *t = thread_current();
  struct open_file *of = get_open_file_by_fd (t, fd);
  if (of != NULL) {
    ret_stat = file_length (of->file);
  }
  return ret_stat;
}


/* Check if the user_ptr is a valid user virtual address or not. */
bool
is_valid_ptr(const void *user_ptr)
{
  struct thread *t = thread_current();
  return user_ptr != NULL && is_user_vaddr(user_ptr)
    && user_ptr >= ((void *) 0x08048000)
    && pagedir_get_page (t->pagedir, user_ptr) != NULL;
}




