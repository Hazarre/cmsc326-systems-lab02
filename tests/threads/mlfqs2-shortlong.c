/* 
   Creates threads that run 1/5 of quantum and whole quantum.  Test
   ability to alternate procs, but stay at one level until quantum
   expires.
*/

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "devices/timer.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

#define NUM_MLFQS 20

struct thread_info 
  {
    int64_t start_time;
    int id;
    int priority;
    int iterations;             /* Iterations so far. */
    int tick_count;
    int runticks;               /* max ticks in one time at CPU */
    int qtimes[NUM_MLFQS];      /* time in each queue */
    int qiters[NUM_MLFQS];      /* iters in each queue */
    //int **op;                   /* Output buffer position. */
    struct lock *lock;          /* Lock on output. */
  };

#define THREAD_CNT 2
#define ITER_CNT 30
#define RUN_TICKS 5

static thread_func test_short;


void
test_mlfqs2_shortlong (void) 
{
  struct thread_info info[THREAD_CNT];
  struct lock lock;
  //int *output, *op;
  int i, j, cnt;

  /* This test is for MLFQS. */
  ASSERT (thread_mlfqs);

  /* Make sure our priority is the default. */
  // ASSERT (thread_get_priority () == PRI_DEFAULT);

  msg ("%d threads will iterate %d times running round robin.",
       THREAD_CNT, ITER_CNT);
  msg ("First thread runs %d ticks then blocks.  Others use entire quantum.",RUN_TICKS);

  //output = op = malloc (sizeof *output * THREAD_CNT * ITER_CNT * 2);
  //ASSERT (output != NULL);
  lock_init (&lock);
  lock_acquire(&lock); // main holds lock
  
  printf("Main at priority %d\n",PRI_MIN);
  thread_set_priority (PRI_MIN); // main at min priority
  for (i = 0; i < THREAD_CNT; i++) 
    {
      for (j = PRI_MAX; j >= PRI_MIN; j--) {
        info[i].qtimes[j] = 0;
        info[i].qiters[j] = 0; 
      }

      char name[16];
      struct thread_info *d = info + i;
      snprintf (name, sizeof name, "%d", i);
      d->tick_count = 0;
      d->id = i;
      d->iterations = 0;
      d->lock = &lock; // all share one lock
      if (i == 0) {
        d->runticks = RUN_TICKS;
        d->priority = PRI_MAX;
        // thread_create (name, PRI_MAX, test_short, d);
      } else {
        d->runticks = 10;
        d->priority = 10;
        // thread_create (name, PRI_MAX-9, test_short, d);
      }

      thread_create (name, PRI_MAX-i, test_short, d);
    }
  //print_mlfqs();
  lock_release(&lock); // all threads can run
  thread_yield();
  
  /* All the other threads now run to termination here. */
  ASSERT (lock.holder == NULL);
  for (i = 0; i < THREAD_CNT; i++) {
    int sum = 0;
    msg ("Thread %d received %d ticks.", i, info[i].tick_count);
    for (j = PRI_MAX; j >= PRI_MIN; j--) {
      msg("Q %3d %6d %6d",j,info[i].qtimes[j],info[i].qiters[j]);
      sum += info[i].qtimes[j];
    }
    msg ("ByQ thread %d received %d ticks.", i, sum);
  }

}

/*
  Run for runticks, then yield.
*/

static void 
test_short(void *info_) 
{
  int i;
  struct thread_info *ti = info_;
  // TIMER_FREQ is 100 and determines tick duration
  struct thread *t = thread_current ();
  int runTicks = ti->runticks;
  thread_set_priority(ti->priority);
  
  int64_t sleep_time = TIMER_FREQ; // one second
  int64_t last_time = 0;
  int64_t start_time;
  int64_t ticksRun = 0;
  
  for (i = 0; i < ITER_CNT; i++) {
    start_time = timer_ticks ();
    last_time = start_time;
    ticksRun = 0;
    // Run for runTicks
    while (ticksRun < runTicks)
      {
        int64_t cur_time = timer_ticks();
        if (cur_time != last_time) { // record another tick
          ti->tick_count++;
          ticksRun++;
          ti->qtimes[thread_get_priority()] += 1; //ticks in this queue
          last_time = cur_time;
        }
      }
    ti->qiters[thread_get_priority()] += 1; // one run completed
    
    thread_yield();
  } // for-loop
}

