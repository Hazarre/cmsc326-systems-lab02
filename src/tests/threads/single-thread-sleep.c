
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

void test_single_thread_sleep ();

void
test_single_thread_sleep(){
  msg ("Sleeps main thread 3 times");
  msg ("Sleeps 1000 ticks each time and prints a message in between");

  timer_sleep (100 * TIMER_FREQ);
  msg("Message One");
  timer_sleep (100 * TIMER_FREQ);
  msg("Message Two");
  timer_sleep (100 * TIMER_FREQ);
  msg("Message Three");
}
