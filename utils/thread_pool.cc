
#include "thread_pool.h"
#include "simthread.h"

thread_pool_t thread_pool_t::the_instance;

thread_pool_t::thread_pool_t()
{
  for (uint8 i = 0; i < std::thread::hardware_concurrency()-1; i++)
  {
    threads.push_back(std::thread([&] {
      while (true) {
        thread_pool_t::the_instance.get_task_from_queue()->run();
      }
    }));
  }
};

void thread_pool_t::add_task_to_queue(std::shared_ptr<runnable_t> task)
{
  {
    std::lock_guard<std::mutex> lock(task_queue_mutex);
    task_queue.push_back(task);
  }
  task_semaphore.post();
};

std::shared_ptr<runnable_t> thread_pool_t::get_task_from_queue()
{
  task_semaphore.wait();
  std::shared_ptr<runnable_t> task;
  {
    std::lock_guard<std::mutex> lock(task_queue_mutex);
    task = task_queue.front();
    task_queue.pop_front();
  }
  return task;
};