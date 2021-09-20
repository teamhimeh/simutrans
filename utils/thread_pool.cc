
#include "thread_pool.h"
#include "simthread.h"

thread_pool_t thread_pool_t::the_instance;

thread_pool_t::thread_pool_t()
{
  quit_threads = false;
  for (uint8 i = 0; i < std::thread::hardware_concurrency()-1; i++)
  {
    threads.push_back(std::thread([&] {
      while (!thread_pool_t::the_instance.quit_threads) {
        auto task = thread_pool_t::the_instance.get_task_from_queue();
        if(task) {
          task->run();
        }
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
    if (task_queue.empty()) {
      return std::shared_ptr<runnable_t>();
    }
    task = task_queue.front();
    task_queue.pop_front();
  }
  return task;
};

void thread_pool_t::terminate_threads()
{
  quit_threads = true;
  task_semaphore.cancel();
  for (uint8 i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
};
