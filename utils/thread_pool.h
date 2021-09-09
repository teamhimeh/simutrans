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

#include <functional>
#include <deque>

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
  pthread_t threads[MAX_THREADS-1];
  simsemaphore_t task_semaphore;
  std::deque<runnable_t *> task_queue;
  pthread_mutex_t task_queue_mutex;

public:
  thread_pool_t();

  void add_task_to_queue(runnable_t *task);

  runnable_t* get_task_from_queue();
};

template<typename T, typename U> 
class dispatch_group_t;

template<typename T, typename U> 
class threaded_task_t : public runnable_t {
public:
  std::function<U(T)> func;
  T parameter;
  U result;
  dispatch_group_t<T, U>* dispatched_from;

  threaded_task_t(std::function<U(T)> func, T parameter, dispatch_group_t<T, U>* df) {
    this->func = func;
    this->parameter = parameter;
    dispatched_from = df;
  }

  virtual ~threaded_task_t() {};

  void run() override {
    result = func(parameter);
    dispatched_from->decrement_count();
  };
};

/*
 * Use this to execute parallel tasks in the thread pool.
 * Do not discard this object until all tasks registered to this are completed.
 * 
 * types
 * T: parameter type
 * U: return value type
 * 
 * TODO: make the object discardable even if all tasks are not completed.
 */

template<typename T, typename U> 
class dispatch_group_t
{
private:
  vector_tpl<threaded_task_t<T, U>*> tasks;
  sint32 task_left_count;
  pthread_mutex_t task_left_count_mutex;
  pthread_cond_t get_results_wait_cond;

public:
  dispatch_group_t() {
    task_left_count = 0;
    pthread_mutex_init(&task_left_count_mutex, NULL);
    pthread_cond_init(&get_results_wait_cond, NULL);
  };

  ~dispatch_group_t() {
    for (uint32 i = 0; i < tasks.get_count(); i++)
    {
      delete tasks[i];
    }
  };

  // The given functions will be executed asynchronously.
  // Lambda expression can be used to pass the function.
  void add_task(std::function<U(T)> func, T parameter) {
    threaded_task_t<T, U> *task = new threaded_task_t<T, U>(func, parameter, this);
    pthread_mutex_lock(&task_left_count_mutex);
    tasks.append(task);
    task_left_count++;
    pthread_mutex_unlock(&task_left_count_mutex);
    thread_pool_t::the_instance.add_task_to_queue(task);
  };

  // wait until all tasks are completed and return the results in vector_tpl type.
  // Do not call this in an async function!
  vector_tpl<U> get_results() {
    pthread_mutex_lock(&task_left_count_mutex);
    while (task_left_count > 0)
    {
      pthread_cond_wait(&get_results_wait_cond, &task_left_count_mutex);
    }
    pthread_mutex_unlock(&task_left_count_mutex);
    vector_tpl<U> results;
    for (uint32 i = 0; i < tasks.get_count(); i++)
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
  void wait_completion() {
    pthread_mutex_lock(&task_left_count_mutex);
    while (task_left_count > 0)
    {
      pthread_cond_wait(&get_results_wait_cond, &task_left_count_mutex);
    }
    pthread_mutex_unlock(&task_left_count_mutex);
  };

  void decrement_count() {
    pthread_mutex_lock(&task_left_count_mutex);
    task_left_count--;
    if (task_left_count <= 0)
    {
      pthread_cond_broadcast(&get_results_wait_cond);
    }
    pthread_mutex_unlock(&task_left_count_mutex);
    };
};

#endif