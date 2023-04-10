/* Creates one cpu-bound thread that runs ~3 seconds.
   @author S. Anderson
*/

#include <stdio.h>
#include <inttypes.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

#define PRIMES_LIMIT 100000 // upper bound for primes search. Tune this
                           // to force each thread to use a
                           // significant fraction of its quantum.

struct thread_info 
  {
    int64_t start_time;
    int id;                     /* Sleeper ID. */
    int iterations;             /* Iterations so far. */
    struct lock *lock;          /* Lock on output. */
    int tick_count;
    int qtimes[NUM_MLFQS];
  };

#define THREAD_CNT 1
#define ITER_CNT 1

static void test_cpubound(void *info_);
void test_mlfqs2_longproc (void) ;

void
test_mlfqs2_longproc (void) 
{
  struct thread_info info[THREAD_CNT];
  struct lock lock;
  int i, j, cnt;
  int64_t start_time;

  ASSERT (thread_mlfqs);
  start_time = timer_ticks ();
  
  msg ("Starting %d cpu-bound thread runs to completion.",THREAD_CNT);
  msg ("Should move down one queue each time.");

  lock_init (&lock);
  lock_acquire(&lock); // main holds lock

  lock_release(&lock);
  
  for (i = 0; i < THREAD_CNT; i++) 
    {
      char name[16];
      struct thread_info *ti = &info[i];
      snprintf (name, sizeof name, "lproc %d", i);
      ti->id = i;
      ti->iterations = 0;
      ti->lock = &lock; // all share one lock
      ti->tick_count = 0;
      ti->start_time = start_time;
      for (j = 0; j < NUM_MLFQS; j++) ti->qtimes[j] = 0;
      thread_create (name, PRI_MAX, test_cpubound, ti);
    }
  msg ("Starting threads took %"PRId64" ticks.",
       timer_elapsed (start_time));


  thread_set_priority (PRI_MIN); // main at min priority

  msg("Sleeping %d ticks to let other threads run.",30*TIMER_FREQ);
  timer_sleep(30 * TIMER_FREQ);

  /* All the other threads now run to termination here. */
  //ASSERT (lock.holder == NULL);
  for (i = 0; i < THREAD_CNT; i++) {
    int sum = 0;
    msg ("Thread %d received %d ticks.", i, info[i].tick_count);
    for (j = PRI_MAX; j >= PRI_MIN; j--) {
      msg("Q %3d %6d",j, info[i].qtimes[j]);
      sum += info[i].qtimes[j];
    }
    msg ("ByQ thread %d received %d ticks.", i, sum);
  }

}

static void 
test_cpubound (void *info_) 
{
  struct thread_info *ti = info_;
  int64_t sleep_time = 5 * TIMER_FREQ;
  int64_t spin_time = sleep_time + 30 * TIMER_FREQ;
  int64_t last_time = 0;

  while (timer_elapsed (ti->start_time) < spin_time) 
    {
      int64_t cur_time = timer_ticks ();
      if (cur_time != last_time) {
        ti->tick_count++;
        ti->qtimes[thread_get_priority()] += 1;
        last_time = cur_time;
      }
    }
}

