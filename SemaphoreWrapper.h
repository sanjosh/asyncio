#pragma once

#include <semaphore.h>


class SemaphoreWrapper
{
  public:
  sem_t semaphore_;

  SemaphoreWrapper()
  {
    int interProcess = 0;
    unsigned int initVal = 0;
    int ret = sem_init(&semaphore_, interProcess, initVal);
    assert(ret == 0);
    (void) ret;
  }

  void pause()
  {
    int ret = sem_wait(&semaphore_);
    assert(ret == 0);
    (void) ret;
  }

  void wakeup(int count = 1)
  {
    for (int i = 0; i < count; i++)
    {
      int ret = sem_post(&semaphore_);
      assert(ret == 0);
      (void) ret;
    }
  }

  int getValue()
  {
    int val = 0;
    int err = sem_getvalue(&semaphore_, &val);
    if (err != 0)
    {
      return val;
    }
    return -errno;
  }

  ~SemaphoreWrapper()
  {
    sem_destroy(&semaphore_);
  }
};

