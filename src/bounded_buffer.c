#include <pthread.h>
#include <stdlib.h>
#include "bounded_buffer.h"
#include "common.h"

typedef struct {
	int fd; // the socket descriptor
	char filename[MAXBUF]; // path to the requested file
	int file_size; // the result of stat(filename)
	int is_static; // whether this is a static file or CGI
	char cgiargs[MAXBUF];
} request_t;

// circular buffer of requests
static request_t *buf;
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
	buf = malloc(sizeof(request_t) * size);
	fill = use = count = 0;
}

// producer. only called in the master thread.
void buffer_put(int fd, int file_size){
	pthread_mutex_lock(&lock);

	while (count == buf_size){
		pthread_cond_wait(&empty, &lock);
	}

	buf[fill].fd = fd;
	buf[fill].file_size = file_size;

	fill = (fill + 1) % buf_size;
	count++;

	pthread_cond_signal(&full);
	pthread_mutex_unlock(&lock);
}

// consumer. called in worker threads
int buffer_get(int *file_size_out){
	pthread_mutex_lock(&lock);

	while (count == 0) {
		pthread_cond_wait(&full, &lock);
	}

	int fd = buf[use].fd;
	if (file_size_out) {
		*file_size_out = buf[use].file_size;
	}

	use = (use + 1) % buf_size;
	count--;

	pthread_cond_signal(&empty);
	pthread_mutex_unlock(&lock);

	return fd;
}
