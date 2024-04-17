#include "thread_pool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct thread_task {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	thread_task_f function;
	void* arg;
	int is_running; // -1 - underfined, 0 - finished, 1 - running
	void* result;
	struct thread_pool* pool;
	struct thread_task* next;
};

struct thread_pool {
	pthread_t* threads;
	int thread_count;
	int active_thread_count;
	int waiting_thread_count;
	bool shutdown;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int task_count;
	struct thread_task* task_next;
	struct thread_task* task_prev;
};

int 
thread_pool_new(int max_thread_count, struct thread_pool** pool) 
{
	if (max_thread_count < 1 || max_thread_count > TPOOL_MAX_THREADS) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	*pool = malloc(sizeof(struct thread_pool));
	if (*pool == NULL) {
		return TPOOL_ERR_NOT_IMPLEMENTED;
	}

	(*pool)->threads = malloc(sizeof(pthread_t) * max_thread_count);

	if ((*pool)->threads == NULL) {
		free(*pool);
		return TPOOL_ERR_NOT_IMPLEMENTED;
	}

	(*pool)->thread_count = max_thread_count;
	(*pool)->active_thread_count = 0;
	(*pool)->waiting_thread_count = 0;
	(*pool)->shutdown = false;
	pthread_mutex_init(&(*pool)->mutex, NULL);
	pthread_cond_init(&(*pool)->cond, NULL);
	(*pool)->task_count = 0;
	(*pool)->task_next = NULL;
	(*pool)->task_prev = NULL;

	return 0;
}

int 
thread_pool_thread_count(const struct thread_pool* pool) 
{
	if (pool == NULL) {
		return 0;
	}
	else {
		return pool->active_thread_count;
	}
}

int 
thread_pool_delete(struct thread_pool* pool) 
{
	if (pool == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	pthread_mutex_lock(&pool->mutex);
	if (pool->active_thread_count != pool->waiting_thread_count || pool->task_count) {
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_HAS_TASKS;
	}

	pool->shutdown = true;
	pthread_cond_broadcast(&pool->cond);

	for (int i = 0; i < pool->active_thread_count; i++) {
		pthread_mutex_unlock(&pool->mutex);
		pthread_join(pool->threads[i], NULL);
	}
	free(pool->threads);

	pthread_mutex_destroy(&pool->mutex);
	pthread_cond_destroy(&pool->cond);
	free(pool);

	return 0;
}

static void* 
handler_thread(void* args) 
{
	struct thread_pool* pool = args;
	pthread_mutex_lock(&pool->mutex);
	pool->waiting_thread_count++;

	while (true) {
		while (!pool->task_count && !pool->shutdown) {
			pthread_cond_wait(&pool->cond, &pool->mutex);
		}
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutex);
			return NULL;
		}

		struct thread_task* task = pool->task_next;
		pool->task_next = task->next;
		if (!pool->task_next) {
			pool->task_prev = NULL;
		}
		task->next = NULL;
		pool->task_count--;

		pthread_mutex_lock(&task->mutex);
		task->is_running = 1;
		pthread_mutex_unlock(&task->mutex);

		task->pool->waiting_thread_count--;
		pthread_mutex_unlock(&task->pool->mutex);

		void* result = task->function(task->arg);

		pthread_mutex_lock(&task->pool->mutex);
		task->pool->waiting_thread_count++;
		pthread_mutex_lock(&task->mutex);

		task->is_running = 0;
		task->result = result;

		pthread_mutex_unlock(&task->mutex);
		pthread_cond_signal(&task->cond);
	}
	return NULL;
}

int 
thread_pool_push_task(struct thread_pool* pool, struct thread_task* task) 
{
	if (pool == NULL || task == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	pthread_mutex_lock(&pool->mutex);
	if (pool->task_count > TPOOL_MAX_TASKS) {
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}

	pthread_mutex_lock(&task->mutex);
	task->pool = pool;

	task->is_running = -1;

	if (!pool->task_next) {
		pool->task_prev = pool->task_next = task;
	}
	else {
		pool->task_prev->next = task;
		pool->task_prev = task;
	}

	pool->task_count++;
	pthread_mutex_unlock(&task->mutex);

	if (pool->waiting_thread_count == 0 && pool->active_thread_count < pool->thread_count) {
		pthread_create(pool->threads + pool->active_thread_count, NULL, handler_thread, pool);
		pool->active_thread_count++;
	}

	pthread_mutex_unlock(&pool->mutex);
	pthread_cond_signal(&pool->cond);

	return 0;
}

int 
thread_task_new(struct thread_task** task, thread_task_f function, void* arg) 
{
	*task = (struct thread_task*)malloc(sizeof(struct thread_task));

	if (*task == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	(*task)->function = function;
	(*task)->arg = arg;
	(*task)->is_running = -1;
	(*task)->pool = NULL;
	(*task)->result = NULL;

	pthread_mutex_init(&(*task)->mutex, NULL);
	pthread_cond_init(&(*task)->cond, NULL);

	(*task)->next = NULL;

	return 0;
}

bool 
thread_task_is_finished(const struct thread_task* task) 
{
	if (task == NULL || task->is_running != 0) {
		return false;
	}
	else {
		return true;
	}
}

bool 
thread_task_is_running(const struct thread_task* task) 
{
	if (task == NULL || task->is_running != 1) {
		return false;
	}
	else {
		return true;
	}
}

int 
thread_task_join(struct thread_task* task, void** result) 
{
	pthread_mutex_lock(&task->mutex);

	if (!task->pool) {
		pthread_mutex_unlock(&task->mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	while (task->is_running != 0) {
		pthread_cond_wait(&task->cond, &task->mutex);
	}

	task->pool = NULL;
	*result = task->result;
	pthread_mutex_unlock(&task->mutex);

	return 0;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_join(struct thread_task* task, void** result) 
{
	int error = 0;

	pthread_mutex_lock(&task->mutex);
	while (task->is_running == 1) {
		pthread_cond_wait(&task->cond, &task->mutex);
	}

	if (task->is_running == 0) {
		*result = task->result;
	}
	else {
		error = TPOOL_ERR_TASK_NOT_PUSHED;
	}

	pthread_mutex_unlock(&task->mutex);

	return error;
}

#endif

int 
thread_task_delete(struct thread_task* task) 
{
	if (task == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	if (task->pool) {
		return TPOOL_ERR_TASK_IN_POOL;
	}

	pthread_mutex_destroy(&task->mutex);
	pthread_cond_destroy(&task->cond);
	free(task);

	return 0;
}

#ifdef NEED_DETACH

int 
thread_task_detach(struct thread_task* task) 
{
	if (task == NULL) {
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}

	pthread_mutex_lock(&task->mutex);

	if (task->is_running == 0) {
		pthread_mutex_unlock(&task->mutex);
		free(task);
		return 0;
	}

	task->pool = NULL;
	pthread_mutex_unlock(&task->mutex);

	return 0;
}

#endif
