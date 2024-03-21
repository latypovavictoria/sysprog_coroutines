#include "thread_pool.h"
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>

struct thread_task {
	pthread_t thread_id;
	thread_task_f function;
	void *arg;
	bool is_finished;
	bool is_running;

	/* PUT HERE OTHER MEMBERS */
};

struct thread_pool {
	pthread_t* threads;
	int thread_count;
	bool shutdown;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct thread_task** tasks;
	int task_count;
};

int 
thread_pool_new(int max_thread_count, struct thread_pool** pool) {
    if (max_thread_count <= 0 || max_thread_count > TPOOL_MAX_THREADS) {
        return TPOOL_ERR_INVALID_ARGUMENT;
    }

    *pool = (struct thread_pool*)malloc(sizeof(struct thread_pool));
    if (*pool == NULL) {
        return TPOOL_ERR_NOT_IMPLEMENTED;
    }

    (*pool)->threads = (pthread_t*)malloc(max_thread_count * sizeof(pthread_t));
    if ((*pool)->threads == NULL) {
        free(*pool);
        return TPOOL_ERR_NOT_IMPLEMENTED;
    }

    (*pool)->thread_count = max_thread_count;
    (*pool)->shutdown = false;
    pthread_mutex_init(&((*pool)->mutex), NULL);
    pthread_cond_init(&((*pool)->cond), NULL);

    (*pool)->tasks = (struct thread_task**)malloc(TPOOL_MAX_TASKS * sizeof(struct thread_task*));
    if ((*pool)->tasks == NULL) {
        free((*pool)->threads);
        free(*pool);
        return TPOOL_ERR_NOT_IMPLEMENTED;
    }

    (*pool)->task_count = 0;

    return 0;
}

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	if (pool == NULL) {
		return 0;
	}
	else {
		return pool->thread_count;
	}
}

int
thread_pool_delete(struct thread_pool *pool)
{
	if (pool == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	pthread_mutex_lock(&pool->mutex);
	if (pool->task_count > 0) {
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_HAS_TASKS;
	}

	pool->shutdown = true;
	pthread_mutex_unlock(&pool->mutex);
	pthread_cond_broadcast(&pool->cond);
	for (int i = 0; i < pool->thread_count; i++) {
		pthread_join(pool->threads[i], NULL);
	}

	free(pool->threads);
	free(pool);
	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	if (pool == NULL || task == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	pthread_mutex_lock(&pool->mutex);

	if (pool->task_count >= TPOOL_MAX_TASKS) {
		pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}

	pool->tasks[pool->task_count++] = task;
	pthread_mutex_unlock(&pool->mutex);
	pthread_cond_signal(&pool->cond);

	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	*task = (struct thread_task*)malloc(sizeof(struct thread_task));
	if (*task == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	(*task)->function = function;
	(*task)->arg = arg;
	(*task)->thread_id++;
	(*task)->is_finished = false;
	(*task)->is_running = true;

	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	if (task == NULL) {
		return false;
	}

	return task->is_finished;
}


bool
thread_task_is_running(const struct thread_task *task)
{
	if (task == NULL) {
		return false;
	}

	return task->is_running;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	if (task == NULL || result == NULL) {
		return -1;
	}

	if (pthread_join(task->thread_id, result) != 0) {
		return -1;
	}

	return 0;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	struct timeval now;
	struct timespec timeout_ts;
	int ret;

	if (!task || !result) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

	if (!task->is_running) {
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}

	gettimeofday(&now, NULL);
	timeout_ts.tv_sec = now.tv_sec + (time_t)timeout;
	timeout_ts.tv_nsec = now.tv_usec * 1000;

	pthread_mutex_lock(&task->mutex);
	while (!task->is_finished) {
		ret = pthread_cond_timedwait(&task->cond, &task->mutex, &timeout_ts);
		if (ret == ETIMEDOUT) {
			pthread_mutex_unlock(&task->mutex);
			return TPOOL_ERR_TIMEOUT;
		}
	}
	pthread_mutex_unlock(&task->mutex);

	*result = task->arg;
	free(task);

	return 0;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
	if (task->is_running) {
		return TPOOL_ERR_TASK_IN_POOL;
	}

	free(task);
	return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	if (task->is_running) {
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	free(task);
	return 0;
}

#endif

