#include <pthread.h>
#include <stdlib.h>
#include "bounded_buffer.h"
#include "common.h"

// circular buffer of requests
static request_info_t *buf;
static int buf_size;
// where the next producer writes
static int fill = 0;
// where the next consumer reads
static int use = 0;
// how many items exist
static int count = 0;

// One mutex protects the shared state.
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
// Two condition vars:
// empty -> signaled when space becomes available.
// full -> signaled when at least one item exists.
static pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t full = PTHREAD_COND_INITIALIZER;

void buffer_init(int size){
	buf_size = size;
	buf = malloc(sizeof(request_info_t) * size);
	fill = use = count = 0;
}

// producer. only called in the master thread.
void buffer_put(request_info_t item, int sff_flag){
	pthread_mutex_lock(&lock);

	while (count == buf_size){
		pthread_cond_wait(&empty, &lock);
	}

	buf[fill] = item;

	if (sff_flag){
		int j = fill;
		while (j != use) {
			int prev = (j - 1 + buf_size) % buf_size;
			if (buf[j].sbuf.st_size >= buf[prev].sbuf.st_size)
				break;

			request_info_t temp = buf[j];
			buf[j] = buf[prev];
			buf[prev] = temp;

			j = prev;
		}
	}

	fill = (fill + 1) % buf_size;
	count++;

	pthread_cond_signal(&full);
	pthread_mutex_unlock(&lock);
}

// consumer. called in worker threads
request_info_t buffer_get(){
	pthread_mutex_lock(&lock);

	while (count == 0) {
		pthread_cond_wait(&full, &lock);
	}

	request_info_t req = buf[use];

	use = (use + 1) % buf_size;
	count--;

	pthread_cond_signal(&empty);
	pthread_mutex_unlock(&lock);

	return req;
}
