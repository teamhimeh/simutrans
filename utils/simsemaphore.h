/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 * 
 * A semaphore that is compatible with C++11
 * POSIX sem_t does not work on macOS.
 * 
 */


#ifndef UTILS_SIMSEMAPHORE_H
#define UTILS_SIMSEMAPHORE_H

#include <atomic>
#include <mutex>
#include <condition_variable>

#include "../simtypes.h"

class simsemaphore_t {
private:
  std::mutex m;
  std::condition_variable cv;
  uint64 cnt;
  std::atomic_bool canceled;

public:
  simsemaphore_t() {
    cnt = 0;
    canceled = false;
  }

  void post() {
    std::lock_guard<std::mutex> lock(m);
    cnt++;
    cv.notify_one();
  }

  void wait() {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [this] {return cnt>0 || canceled;});
    cnt--;
  }

  void cancel() {
    canceled = true;
    cv.notify_all();
  }
};

#endif