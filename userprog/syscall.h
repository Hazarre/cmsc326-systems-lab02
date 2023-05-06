#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"


#define ERROR -1
#define NOT_LOADED 0

void syscall_init (void);



/* 
 You must synchronize system calls so that any number of user
 processes can make them at once. In particular, it is not safe to
 call into the file system code provided in the filesys directory from
 multiple threads at once. Your system call implementation must treat
 the file system code as a critical section.
*/

struct child_process {
  int pid;
  int load;
  bool wait;
  bool exit;
  int status;
  struct lock wait_lock;
  struct list_elem elem;
};

// Represent file within a process (kept in list) via fid
struct file_in_proc {
  struct file *file;
  int fd; // file handle
  struct list_elem elem;
};

extern struct lock file_lock;  //+ only one thread can access filesys
			       //+ code

#endif /* userprog/syscall.h */
