/*
 * g++ <prog>.cpp -lpthread -laio -std=c++11
 */
#include <libaio.h>
#include <stdlib.h>//malloc
#include <future> //async
#include <unistd.h> //usleep
#include <iostream>
#include <vector>
#include <fcntl.h> // open
#include <errno.h>
#include <sys/types.h> // open
#include <sys/stat.h> // open
#include <cassert>
#include "SemaphoreWrapper.h"
#include <chrono>

typedef std::chrono::high_resolution_clock Clock;

io_context_t ioctx;
std::vector<char*> buffers;
int fd = -1;
bool terminated = false;
int numSubmitted = 0;

uint64_t totalFileSize = ((uint64_t)1) << 33; // 64GB file
uint64_t blockSize = 65536;
uint64_t numPerRound = 64;
uint64_t numRounds = 0;
int32_t ctxDepth = 20;

SemaphoreWrapper sem;

void DoGet()
{
  io_event eventsArray[10];
  int numCompleted = 0;

  usleep(100000);

  while (numCompleted != numSubmitted)
  {
    bzero(eventsArray, 10 * sizeof(io_event));
    int numEvents;
    do {
      numEvents = io_getevents(ioctx, 1, 10, eventsArray, nullptr);
    } while ((numEvents == -EINTR) || (numEvents == -EAGAIN));

    for (int i = 0; i < numEvents; i++)
    {
      io_event* ev = &eventsArray[i];
      iocb* cb = (iocb*)(ev->data);
      sem.wakeup();
    }
    numCompleted += numEvents;
  }
}

int main(int argc, char* argv[])
{
  int osync = 0;

  if (argc == 1) {
    std::cerr << argv[0] << " <osync> <blockSize> " << std::endl;
    exit(1);
  }

  osync = atoi(argv[1]);
  blockSize = atoi(argv[2]);

  numRounds = totalFileSize/(numPerRound * blockSize);

  io_queue_init(ctxDepth, &ioctx);
  sem.wakeup(numPerRound);

  numSubmitted = numRounds * numPerRound;

  auto DoGetFut = std::async(std::launch::async, DoGet);

  for (int i = 0; i < 26; i++)
  {
    char* buf;
    int ret = posix_memalign((void**)&buf, 4096, blockSize);
    assert(ret == 0);
    assert(buf != nullptr);
    memset(buf, 'a' + (i%26), blockSize);
    buffers.push_back(buf);
  }
  
  int flags = O_CREAT | O_RDWR | O_TRUNC | O_DIRECT;
  if (osync) { 
    flags |= O_SYNC; 
  }

  fd = open("/mnt/ioexec_test", flags,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  assert(fd >= 0);

  Clock::time_point start = Clock::now();

  off_t off = 0;

  for (uint64_t j = 0; j < numRounds; j++)
  {
    iocb mycb[numPerRound];
    iocb* posted[numPerRound];

    bzero(mycb, sizeof(iocb) * numPerRound);


    for (uint64_t i = 0; i < numPerRound; i++)
    {
      char* iobuf = buffers[(i + j) % 26];
      iocb* cb = &mycb[i];

      sem.pause();
      io_prep_pwrite(cb, fd, iobuf, blockSize, off);
      off += blockSize;
      cb->data = iobuf;
      //std::cout << "cb=" << (void*)cb << ":inbuf=" << (void*)cb->data << std::endl;
      posted[i] = cb;
    }


    int ret = 0;
    iocb** iocbptr = &posted[0];
    long nr = numPerRound; 
    do {
      ret = io_submit(ioctx, nr, iocbptr);

      if ((ret > 0) && ( ret != nr)) {
        std::cerr << "round=" << j << ":submitted=" << ret << std::endl;
        nr -= ret;
        iocbptr += ret;
      } else if ((ret < 0) && (ret != -EINTR && ret != -EAGAIN)) {
        std::cerr << "round=" << j << ":errno=" << ret << std::endl;
        exit(1);
      } else if (ret == nr ) {
        break;
      }
    } while (1);

    //std::cout << "done round=" << j << ":off=" << off << std::endl;
  }

  DoGetFut.wait();

  Clock::time_point end = Clock::now();

  close(fd);

  io_queue_release(ioctx);

  uint64_t diffMicro = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  std::cout 
    << osync
    << "," << blockSize 
    << "," << totalFileSize/diffMicro
    << std::endl;

}
