#include <stdio.h>

#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"

#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"

// sys call stuff
void syscall_init (void);
static void syscall_handler (struct intr_frame *);
void get_args(void *sp, void **args,int n);

// Proc control
void s_exit (int status);
void s_halt (void);
tid_t s_exec(char *fname);

// Filesys
int s_write(int fd,char *buf,unsigned bufsize);
int s_read(int fd,char *buf,unsigned bufsize);

bool s_create(const char *,unsigned);
bool s_remove(const char *);
int s_open(const char *);
void s_close(int fid);
unsigned s_tell(int fid);
int s_filesize(int fid);

static bool verify_user (const uint8_t *uaddr);
struct child_process* add_child_process (int pid);
struct child_process* get_child_process (int pid);
void remove_child_process (struct child_process *cp);

// Memory checks (needs work)
void check_invalid_ptr_error (const void *vaddr);
void check_buffer_ptr(const void *buf,unsigned size);
int user_to_kernel_ptr(const void *vaddr);

// file misc
struct file *get_file(int fd);
//int add_file(struct file *fp);
void process_close_file (int fd);

static void *s_args[5]; // getting args from functions
#define USER_VADDR_BASE ((void *) 0x08048000)
#define MAX_BUFWRITE 256
#define MIN(a,b) ((a)<(b)?(a):(b))

struct lock file_lock;  //+ only one thread can access filesys code

/* Initialization for system calls */
void
syscall_init (void) 
{
   lock_init(&file_lock);
   intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*
  Handle each system call.
 intr_frame carries syscal args
*/
static void
syscall_handler (struct intr_frame *f ) // UNUSED) 
{
  check_buffer_ptr(f->esp,4); // for sc reads over page boundary

  int call_num = *(int *)(f->esp);
  //printf ("system call nr: %d\n",call_num);
 switch (call_num) {

 case SYS_HALT:
    s_halt();
    break;

 case SYS_EXIT:
    get_args(f->esp,(void **)&s_args,1);
    int status = (int) (s_args[0]);
    s_exit (status);
    break;

 case SYS_EXEC:
    // printf("Got to sys_exec\n");
    get_args(f->esp,(void **)&s_args,1);
    check_invalid_ptr_error((const void *) s_args[0]);
    char *fname = (char *)(user_to_kernel_ptr((const void *) s_args[0]));
    f->eax = (tid_t) s_exec(fname);
    break;

  case SYS_WAIT: // lab 4: add wait syscall
    get_args(f->esp,(void **)&s_args,1);
    int pid = (tid_t) (s_args[0]);
    f->eax = s_wait(pid); 
    break;

 case SYS_FILESIZE:
   get_args(f->esp,(void **)&s_args,1);
   int fid_fsize = (int) (s_args[0]);
   f->eax = s_filesize(fid_fsize);
   break;

 case SYS_REMOVE:
    get_args(f->esp,(void **) &s_args,1);
    s_args[0] = (void *) user_to_kernel_ptr((const void *) s_args[0]);
    fname = (char *)(s_args[0]);
    f->eax = s_remove(fname);
    break;

 case SYS_OPEN:
   get_args(f->esp,(void **)&s_args,1);
   if ((char *) s_args[0] == NULL) s_exit(ERROR);
   check_buffer_ptr(s_args[0],1);
   s_args[0] = (void *) user_to_kernel_ptr((const void *) s_args[0]);
   fname = (char *)(s_args[0]);
   f->eax = s_open(fname);
   break;

 case SYS_CREATE:
    get_args(f->esp,(void **)&s_args,2);
    unsigned int size = (unsigned int)(s_args[1]);
    check_buffer_ptr(s_args[0],size);
    s_args[0] = (void *) user_to_kernel_ptr(s_args[0]);
    fname = (char *)(s_args[0]);
    f->eax = s_create(fname,size);
    break;

  case SYS_READ:
  case SYS_WRITE:
    get_args(f->esp,(void **)&s_args,3);
    int fd_rw = (int)(s_args[0]);
    unsigned int bufsize = (unsigned int)(s_args[2]);
    check_buffer_ptr((char *)(s_args[1]),bufsize);
    char *buf = (char *) user_to_kernel_ptr((const void *) s_args[1]);

    if (call_num == SYS_WRITE) 
      f->eax = s_write(fd_rw,buf,bufsize);
    else
      f->eax = s_read(fd_rw,buf,bufsize);      
    break;

   case SYS_CLOSE:
     get_args(f->esp,(void **)&s_args,1);
     int fid = (int)(s_args[0]);
     s_close(fid);
     break;
    
 default:
    printf("Sys call is unknown!!\n");
 }

}

/*
  Get n function args from stack pointer.
  Validate each pointer.
  
*/
void get_args(void *sp, void **args,int n) {
  int i;
  int *p;

  if (n <= 0) return;
  for (i = 0; i < n; i++) {
    p = (int *) sp + (i + 1); // forces pointer offsets to 4 bytes!
    check_invalid_ptr_error((const void *) p); // check stack ptr
    //check_invalid_ptr_error((const void *) *p); // check dereferenced stack ptr
    args[i] = (void *) *p;//(sp + (i+1) * wlen);
    //printf("Args %d: %x\n",i,*(int*)args[i]);
  }
}

void s_halt () {
  shutdown_power_off();
}

/*
  return size of file in bytes
*/
int s_filesize(int fid) {
  int size = 0;
  struct file *fp = get_file(fid);
  if (fp != NULL) {
    lock_acquire(&file_lock);    
    size = file_length(fp);
    lock_release(&file_lock);    
    free(fp);
  }
  return size;
}


/*
System Call: pid_t exec (const char *cmd_line)

Runs the executable whose name is given in cmd_line, passing any given
arguments, and returns the new process's program id (pid). Must return
pid -1, which otherwise should not be a valid pid, if the program
cannot load or run for any reason. Thus, the parent process cannot
return from the exec until it knows whether the child process
successfully loaded its executable. You must use appropriate
synchronization to ensure this.

*/
tid_t
s_exec(char *cmdline) {
  tid_t pid = process_execute(cmdline);

  if (pid == TID_ERROR) return (ERROR);
  struct child_process* cp = get_child_process(pid);
  ASSERT(cp);
  return pid;
}

int 
s_wait(tid_t pid)
{
  //lab4
  return process_wait(pid);
}

/*  
   Terminates the current user program, returning status to the
   kernel. If the process's parent waits for it (see below), this is
   the status that will be returned. Conventionally, a status of 0
   indicates success and nonzero values indicate errors.
*/
void s_exit (int status) {
  struct thread *curthread = thread_current();

  if (thread_alive(curthread->parent))
  {
    curthread->cp->status = status; // set up child status for later
  }
  
  printf ("%s: exit(%d)\n",curthread->name,status);
  if (lock_held_by_current_thread(&file_lock)) lock_release(&file_lock);
  
  thread_exit();
}

/*
  System Call: bool create (const char *file, unsigned initial_size)
*/
bool s_create(const char *fname, unsigned size) {
  if (fname == NULL) s_exit(ERROR);
  lock_acquire(&file_lock);
  bool status = filesys_create(fname,size);
  lock_release(&file_lock);
  return status;
}


/*
  syscall: open file
  Multiple procs can open a file, each with difft fid.
*/
int s_open(const char *fname) {
  struct file *file = NULL;
  lock_acquire(&file_lock);
  file = filesys_open(fname);
  if (file == NULL) 
    {
      lock_release(&file_lock);
      return -1;
    }
  lock_release(&file_lock);
  int fd = add_file(file);
  return fd;
}

/*
  +: Add file to process.
*/
int add_file(struct file *fp) {
  struct file_in_proc *flist = malloc(sizeof(struct file_in_proc));
  flist->file = fp;
  flist->fd = thread_current()->fd;
  thread_current()->fd++;
  list_push_back(&thread_current()->file_list, &flist->elem);
  return flist->fd;
}


/*
  syscall: int write (int fd, const void *buffer, unsigned size)

Writes size bytes from buffer to the open file fd. Returns the number
of bytes actually written, which may be less than size if some bytes
could not be written.  

Writing past end-of-file would normally extend the file, but file
growth is not implemented by the basic file system. The expected
behavior is to write as many bytes as possible up to end-of-file and
return the actual number written, or 0 if no bytes could be written at
all.

Fd 1 writes to the console. Your code to write to the console should
write all of buffer in one call to putbuf(), at least as long as size
is not bigger than a few hundred bytes. (It is reasonable to break up
larger buffers.) Otherwise, lines of text output by different
processes may end up interleaved on the console, confusing both human
readers and our grading scripts.

*/
int s_write(int fd,char *buf,unsigned bufsize) {
  unsigned nwritten = 0;
  unsigned numtowrite = 0;

  if (fd == 1) { // to stdout
    while (nwritten < bufsize) {
      numtowrite = MIN(bufsize-nwritten,MAX_BUFWRITE);
      putbuf(buf,numtowrite);
      nwritten += numtowrite;
    }

  } else {
    struct file *fp = get_file(fd);
    if (fp == NULL) return nwritten;
    lock_acquire(&file_lock);
    nwritten = file_write(fp,buf,bufsize);
    lock_release(&file_lock);
  }
  return nwritten;
}


/*
   Reads size bytes from the file open as fd into buffer. Returns the
   number of bytes actually read (0 at end of file), or -1 if the file
   could not be read (due to a condition other than end of file). Fd 0
   reads from the keyboard using input_getc().
*/
int s_read(int fd,char *buf,unsigned bufsize) {
  int nread = 0;

  if (fd == 0) { // from stdin
    for (nread = 0; nread < bufsize; nread++) {
      buf[nread] = (char) input_getc();
    }
  }
  else {
    struct file *fp = get_file(fd);
    if (fp == NULL) {
      return nread;
    }
    lock_acquire(&file_lock);
    nread = (int) file_read(fp,buf,bufsize);
    lock_release(&file_lock);
  }
  return nread;
}

/**
   check that from buf to buf+size is within
   thread's pagedir.
   
 */
void check_buffer_ptr(const void *buf,unsigned size) {
  char *ptr = (char *) buf;
  buf += size;
  uint32_t *pd = thread_current()->pagedir;

  while (ptr < (char *) buf) {
    check_invalid_ptr_error(ptr);
    if (pagedir_get_page(pd,ptr) == NULL) s_exit(ERROR);
    ptr++;
  }
}

/*
  Check validity of virtual address for user space.
*/
void check_invalid_ptr_error (const void *vaddr)
{
  // lab 4: want cond true when vaddr invalid 
  if (!is_user_vaddr(vaddr) || vaddr < USER_VADDR_BASE)
    {
      s_exit(ERROR);
    }
}


int user_to_kernel_ptr(const void *vaddr)
{
  check_invalid_ptr_error(vaddr);
  void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
  if (!ptr)
    {
      s_exit(ERROR);
    }
  return (int) ptr;
}


/*
  Get file belonging to current thread via its fd.
*/
struct file *get_file(int fd) {
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->file_list);
       e != list_end (&t->file_list);
       e = list_next (e))
    {
      struct file_in_proc *pf = list_entry (e, struct file_in_proc, elem);
      if (fd == pf->fd)
        {
          return pf->file;
        }
    }
  return NULL;
}

/*
  Close all files associated with a thread.
*/
void process_close_file (int fd)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->file_list);

  while (e != list_end (&t->file_list))
    {
      next = list_next(e);
      struct file_in_proc *pf = list_entry (e, struct file_in_proc, elem);
      if (fd == pf->fd ) // || fd == CLOSE_ALL)
        {
          lock_acquire(&file_lock);    
          file_close(pf->file);
          lock_release(&file_lock);    
          list_remove(&pf->elem);
          free(pf);
          if (fd != 27) //CLOSE_ALL)
            {
              return;
            }
        }
      e = next;
    }
}


/*
 */
void s_close(int fid) {
  process_close_file(fid);
}


/*
System Call: bool remove (const char *file)

Deletes the file called file. Returns true if successful, false
otherwise. A file may be removed regardless of whether it is open or
closed, and removing an open file does not close it. See Removing an
Open File, for details.

*/
bool s_remove(const char* fname) {
  return filesys_remove(fname);
}



/* 
   Validate data user virtual address uaddr.
   UADDR must be below PHYS_BASE.
   It must not be null.
   It must be a mapped address.
   Returns true if successful, false if not.

*/
static bool 
verify_user (const uint8_t *uaddr)
{
  if (uaddr == NULL || (void *) uaddr >= PHYS_BASE) return false;
  // is it mapped?  How do I check this?
  return true;
}


struct child_process* add_child_process (int pid)
{
  struct child_process* cp = malloc(sizeof(struct child_process));
  cp->pid = pid;
  cp->load = NOT_LOADED;
  cp->wait = false;
  cp->exit = false;
  cp->status = 0;
  // lab4 
  sema_init(&(cp->wait_sema),0);

  list_push_back(&thread_current()->child_list,&cp->elem);
  return cp;
}

/*
  Returns struct of child process with pid.
  NULL if failed to find.
*/
struct child_process* get_child_process (int pid)
{
  struct thread *t = thread_current();
  struct list_elem *e;
  // examine child processes, return matching pid.
  for (e = list_begin (&t->child_list); e != list_end (&t->child_list);
       e = list_next (e))
        {
          struct child_process *cp = list_entry (e, struct child_process, elem);
          if (pid == cp->pid)
            {
              return cp;
            }
        }
  return NULL;
}

void remove_child_process (struct child_process *cp)
{
  list_remove(&cp->elem);
  free(cp);
}


void remove_child_processes(void) {
 
  //lab 4
  // loop over child processes and remove each one
   struct thread *t = thread_current();
  struct list_elem *e;
  // examine child processes, return matching pid.
  e = list_begin (&t->child_list);
  if (e != list_head(&t->child_list))
  {
    while (e != list_tail (&t->child_list))
    {
      struct child_process *cp = list_entry (e, struct child_process, elem);
      remove_child_process(cp);
      e = list_next (e);
    }
  }
}

