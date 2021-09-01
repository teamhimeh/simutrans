
#include "thread_pool.h"
#include "simthread.h"

thread_pool_t thread_pool_t::the_instance;

void *thread_worker(void *)
{
  while (true)
  {
    thread_pool_t::the_instance.get_task_from_queue()->run();\
  }
  return NULL;
};

thread_pool_t::thread_pool_t()
{
  pthread_mutex_init(&task_queue_mutex, NULL);
  sem_init(&task_semaphore, 0, 0);
  for (uint8 i = 0; i < MAX_THREADS-1; i++)
  {
    pthread_create(&threads[i], NULL, &thread_worker, NULL);
  }
};

void thread_pool_t::add_task_to_queue(runnable_t *task)
{
  pthread_mutex_lock(&task_queue_mutex);
  task_queue.push_back(task);
  pthread_mutex_unlock(&task_queue_mutex);
  sem_post(&task_semaphore);
};

runnable_t *thread_pool_t::get_task_from_queue()
{
  sem_wait(&task_semaphore);
  pthread_mutex_lock(&task_queue_mutex);
  runnable_t *task = task_queue.front();
  task_queue.pop_front();
  pthread_mutex_unlock(&task_queue_mutex);
  return task;
};