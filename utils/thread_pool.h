/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 * 
 * Use dispatch_group_t to use thread pool.
 * Do not use thread_pool_t directory.
 * 
 */

#ifndef UTILS_THREAD_POOL_H
#define UTILS_THREAD_POOL_H

#include <condition_variable>
#include <functional>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../tpl/vector_tpl.h"
#include "../simconst.h"
#include "simthread.h"
#include "thread_pool.h"
#include "simsemaphore.h"
#include "simthread.h"

class runnable_t {
public:
  virtual void run() = 0;
};

class thread_pool_t
{
public:
  static thread_pool_t the_instance;

private:
  std::vector<std::thread> threads;
  simsemaphore_t task_semaphore;
  std::deque<std::shared_ptr<runnable_t>> task_queue;
  std::mutex task_queue_mutex;

public:
  thread_pool_t();

  void add_task_to_queue(std::shared_ptr<runnable_t> task);

  std::shared_ptr<runnable_t> get_task_from_queue();
};

template<typename T, typename U> 
class dispatch_group_t;

template<typename T, typename U> 
class threaded_task_t : public runnable_t {
public:
  std::function<U(T)> func;
  T parameter;
  U result;
  std::weak_ptr<dispatch_group_t<T, U>> dispatched_from;

  threaded_task_t(std::function<U(T)> func, T parameter, std::shared_ptr<dispatch_group_t<T, U>> df) {
    this->func = func;
    this->parameter = parameter;
    dispatched_from = df;
  }

  void run() override {
    result = func(parameter);
    if (std::shared_ptr<dispatch_group_t<T, U>> df = dispatched_from.lock()) {
      df->decrement_count();
    }
  };
};



/**
 * An interface class to provide type independent functions of dispatch_group_t
 */
class dispatch_group_base_t {
public:
  virtual void wait_completion() = 0;
};


/*
 * Use this to execute parallel tasks in the thread pool.
 * Always manage the object with std::shared_ptr.
 * 
 * types
 * T: parameter type
 * U: return value type
 * 
 * TODO: make the object discardable even if all tasks are not completed.
 */
template<typename T, typename U> 
class dispatch_group_t : public dispatch_group_base_t, public std::enable_shared_from_this<dispatch_group_t<T, U>>
{
private:
  std::vector<std::shared_ptr<threaded_task_t<T, U>>> tasks;
  sint32 task_left_count;
  std::mutex task_left_count_mutex;
  std::condition_variable get_results_wait_cond;

public:
  dispatch_group_t() {
    task_left_count = 0;
  };

  // The given functions will be executed asynchronously.
  // Lambda expression can be used to pass the function.
  void add_task(std::function<U(T)> func, T parameter) {
    std::shared_ptr<threaded_task_t<T, U>> task(new threaded_task_t<T, U>(func, parameter, this->shared_from_this()));
    {
      std::lock_guard<std::mutex> lock(task_left_count_mutex);
      tasks.push_back(task);
      task_left_count++;
    }
    thread_pool_t::the_instance.add_task_to_queue(task);
  };

  // wait until all tasks are completed and return the results in vector_tpl type.
  // Do not call this in an async function!
  vector_tpl<U> get_results() {
    {
      std::unique_lock<std::mutex> lk(task_left_count_mutex);
      get_results_wait_cond.wait(lk, [this] { return task_left_count <= 0; });
    }
    vector_tpl<U> results;
    for (uint32 i = 0; i < tasks.size(); i++)
    {
      if (tasks[i] && tasks[i]->result)
      {
        results.append(tasks[i]->result);
      }
    }
    return results;
  };

  // just wait until all tasks are completed.
  // Do not call this in an async function!
  void wait_completion() override {
    std::unique_lock<std::mutex> lk(task_left_count_mutex);
    get_results_wait_cond.wait(lk, [this] { return task_left_count <= 0; });
  };

  void decrement_count() {
    std::lock_guard<std::mutex> lock(task_left_count_mutex);
    task_left_count--;
    if (task_left_count <= 0) {
      get_results_wait_cond.notify_all();
    }
  };
};

#endif