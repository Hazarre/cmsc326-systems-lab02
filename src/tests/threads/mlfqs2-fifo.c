/* Creates several threads all at the same priority and ensures
   that they consistently run in the same round-robin order.

   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by by Matt Franklin
   <startled@leland.stanford.edu>, Greg Hutchins
   <gmh@leland.stanford.edu>, Yu Ping Hu <yph@cs.stanford.edu>.
   Modified by arens. 
   
   Modified for Bard CMSC 326 by S. Anderson
*/

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "devices/timer.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

struct simple_thread_data 
  {
    int id;                     /* Sleeper ID. */
    int iterations;             /* Iterations so far. */
    struct lock *lock;          /* Lock on output. */
    int **op;                   /* Output buffer position. */
  };

#define THREAD_CNT 10
#define ITER_CNT 10

static thread_func simple_thread_func;

void
test_mlfqs2_fifo (void) 
{
  struct simple_thread_data data[THREAD_CNT];
  struct lock lock;
  int *output, *op;
  int i, cnt;

  /* This test does not work with the MLFQS. */
  // ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  // ASSERT (thread_get_priority () == PRI_DEFAULT);

  msg ("%d threads will iterate %d times running round robin.",
       THREAD_CNT, ITER_CNT);
  msg ("If the order varies then there is a bug.");

  output = op = malloc (sizeof *output * THREAD_CNT * ITER_CNT * 2);
  ASSERT (output != NULL);
  lock_init (&lock);
  lock_acquire(&lock); // main holds lock
  
  printf("All threads at priority %d\n",PRI_DEFAULT - 2);
  thread_set_priority (PRI_MIN); // main at min priority
  for (i = 0; i < THREAD_CNT; i++) 
    {
      char name[16];
      struct simple_thread_data *d = data + i;
      snprintf (name, sizeof name, "%d", i);
      d->id = i;
      d->iterations = 0;
      d->lock = &lock; // all share one lock
      d->op = &op;
      thread_create (name, PRI_DEFAULT - 2, simple_thread_func, d);
    }
  //print_mlfqs();
  lock_release(&lock); // all threads can run
  thread_yield();
  
  /* All the other threads now run to termination here. */
  ASSERT (lock.holder == NULL);

  cnt = 0;
  for (; output < op; output++) 
    {
      struct simple_thread_data *d;

      ASSERT (*output >= 0 && *output < THREAD_CNT);
      d = data + *output;
      if (cnt % THREAD_CNT == 0)
        printf ("(mlfqs2-fifo) iteration:");
      printf (" %d", d->id);
      if (++cnt % THREAD_CNT == 0)
        printf ("\n");
      d->iterations++;
    }
  //  printf("XX %s\n",infobuf);
}

static void 
simple_thread_func (void *data_) 
{
  struct simple_thread_data *data = data_;
  int i;

  for (i = 0; i < ITER_CNT; i++) 
    {
      lock_acquire (data->lock);

      *(*data->op)++ = data->id;
      lock_release (data->lock);
      thread_yield ();
    }
}
