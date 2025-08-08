/*
 * This file is part of the Simutrans project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "thread_pool.h"

#ifdef MULTI_THREAD
#include <unistd.h>

thread_pool_t::thread_pool_t(int thread_count) : stop(false), active_tasks(0), pending_tasks(0), suspended_tasks(0), sync_requested(false)
{
	pthread_mutex_init(&queue_mutex, nullptr);
	pthread_cond_init(&condition, nullptr);
	pthread_cond_init(&finished_condition, nullptr);
	pthread_cond_init(&suspend_condition, nullptr);
	pthread_cond_init(&all_suspended_condition, nullptr);

	workers.reserve(thread_count);
	for (int i = 0; i < thread_count; ++i) {
		pthread_t worker;
		pthread_create(&worker, nullptr, worker_thread, this);
		workers.push_back(worker);
	}
}

thread_pool_t::~thread_pool_t()
{
	// Signal all threads to stop
	pthread_mutex_lock(&queue_mutex);
	stop = true;
	pthread_cond_broadcast(&condition);
	pthread_mutex_unlock(&queue_mutex);

	// Wait for all threads to finish
	for (pthread_t worker : workers) {
		pthread_join(worker, nullptr);
	}

	// Clean up
	pthread_mutex_destroy(&queue_mutex);
	pthread_cond_destroy(&condition);
	pthread_cond_destroy(&finished_condition);
	pthread_cond_destroy(&suspend_condition);
	pthread_cond_destroy(&all_suspended_condition);
}

void* thread_pool_t::worker_thread(void* arg)
{
	thread_pool_t* pool = static_cast<thread_pool_t*>(arg);
	pool->worker_loop();
	return nullptr;
}

void thread_pool_t::worker_loop()
{
	while (true) {
		task_t task([](std::function<void()>){});
		bool has_task = false;

		// Get next task from queue
		pthread_mutex_lock(&queue_mutex);
		while (tasks.empty() && !stop) {
			pthread_cond_wait(&condition, &queue_mutex);
		}

		if (stop && tasks.empty()) {
			pthread_mutex_unlock(&queue_mutex);
			break;
		}

		if (!tasks.empty()) {
			task = tasks.front();
			tasks.pop();
			pending_tasks--;
			active_tasks++;
			has_task = true;
		}

		pthread_mutex_unlock(&queue_mutex);

		// Execute task
		if (has_task) {
			// Create suspend function for this task
			std::function<void()> suspend_func = [this]() {
				pthread_mutex_lock(&queue_mutex);
				suspended_tasks++;
				active_tasks--;
				
				// Signal if all tasks are suspended
				if (active_tasks == 0) {
					pthread_cond_broadcast(&all_suspended_condition);
				}
				
				// Wait until sync is done
				while (sync_requested) {
					pthread_cond_wait(&suspend_condition, &queue_mutex);
				}
				
				suspended_tasks--;
				active_tasks++;
				pthread_mutex_unlock(&queue_mutex);
			};

			task.func(suspend_func);

			// Mark task as completed
			pthread_mutex_lock(&queue_mutex);
			active_tasks--;
			if (active_tasks == 0 && pending_tasks == 0) {
				pthread_cond_broadcast(&finished_condition);
			}
			// Also signal all_suspended_condition in case wait_for_all_with_interrupt is waiting
			if (active_tasks == 0) {
				pthread_cond_broadcast(&all_suspended_condition);
			}
			pthread_mutex_unlock(&queue_mutex);
		}
	}
}

void thread_pool_t::enqueue(std::function<void(std::function<void()>)> func)
{
	pthread_mutex_lock(&queue_mutex);
	
	if (!stop) {
		tasks.emplace(func);
		pending_tasks++;
		pthread_cond_signal(&condition);
	}
	
	pthread_mutex_unlock(&queue_mutex);
}

void thread_pool_t::wait_for_all()
{
	pthread_mutex_lock(&queue_mutex);
	
	while (active_tasks > 0 || pending_tasks > 0) {
		pthread_cond_wait(&finished_condition, &queue_mutex);
	}
	
	pthread_mutex_unlock(&queue_mutex);
}

void thread_pool_t::wait_for_all_with_interrupt(std::function<bool()> interrupt_func, std::function<void()> sync_func)
{
	pthread_mutex_lock(&queue_mutex);
	
	while (active_tasks > 0 || pending_tasks > 0 || suspended_tasks > 0) {
		// Check if we need to interrupt for sync
		if (interrupt_func()) {
			// Only proceed with sync if there are actually tasks running
			if (active_tasks > 0 || pending_tasks > 0) {
				// Request sync - this will cause tasks to suspend when they call suspend_func
				sync_requested = true;
				
				// Wait for all currently active tasks to suspend or complete
				while (active_tasks > 0) {
					pthread_cond_wait(&all_suspended_condition, &queue_mutex);
				}
				
				pthread_mutex_unlock(&queue_mutex);
				
				// Perform sync while all tasks are suspended
				sync_func();
				
				// Resume all suspended tasks
				pthread_mutex_lock(&queue_mutex);
				sync_requested = false;
				pthread_cond_broadcast(&suspend_condition);
			}
			// If no tasks are running, ignore the interrupt request and continue
		} else {
			// Wait for tasks to complete, but with timeout to check interrupt_func periodically
			struct timespec timeout;
			clock_gettime(CLOCK_REALTIME, &timeout);
			timeout.tv_nsec += 10000000; // 10ms timeout
			if (timeout.tv_nsec >= 1000000000) {
				timeout.tv_sec += 1;
				timeout.tv_nsec -= 1000000000;
			}
			
			pthread_cond_timedwait(&finished_condition, &queue_mutex, &timeout);
			// Continue loop to check interrupt_func again
		}
	}
	
	pthread_mutex_unlock(&queue_mutex);
}

#endif