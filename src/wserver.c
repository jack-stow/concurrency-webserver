#include <stdio.h>
#include "request.h"
#include "io_helper.h"
#include "bounded_buffer.h"

char default_root[] = ".";

int *buffer;
int buffer_size;

void *worker(void *arg) {
	while (1) {
		int file_size;
		// get a connection from buffer
		int conn_fd = buffer_get(&file_size);
		// handle HTTP request
		request_handle(conn_fd);
		close_or_die(conn_fd);
	}
}


//
// ./wserver [-d <basedir>] [-p <portnum>]
//
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
    int threads = 1;
    int buffers = 1;
    char *schedalg = "FIFO";

    while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
		switch (c) {
			case 'd':
	    		root_dir = optarg;
	    		break;
			case 'p':
	    		port = atoi(optarg);
	    		break;
			case 't':
				threads = atoi(optarg);
				if (buffers <= 0){
					fprintf(stderr, "Error: Invalid number of threads '%s'. threads must be a positive integer\n", optarg);
					fprintf(stderr, "usage: wserver [-d baseddir] [-p port] [-t threads] [-b buffersize] [-s policy]\n");
				}
				break;
			case 'b':
				buffers = atoi(optarg);
				if (buffers <= 0){
					fprintf(stderr, "Error: Invalid buffer size '%s'. buffer must be a positive integer\n", optarg);
					fprintf(stderr, "usage: wserver [-d baseddir] [-p port] [-t threads] [-b buffersize] [-s policy]\n");
				}
				break;
			case 's':
				if (strcmp(optarg, "FIFO") == 0 || strcmp(optarg, "SFF") == 0){
					schedalg = optarg;
				} else {
					fprintf(stderr, "Error: Invalid schedulign algorithm '%s'. Must be either FIFO or SFF.\n", optarg);
					fprintf(stderr, "usage: wserver [-d baseddir] [-p port] [-t threads] [-b buffersize] [-s policy]\n");
					exit(1);
				}
				schedalg = optarg;
				break;
			default:
	    		fprintf(stderr, "usage: wserver [-d baseddir] [-p port] [-t threads] [-b buffersize] [-s policy]\n");
	    		exit(1);
		}
	printf("num threads: %d", threads);
	printf("buffer size: %d", buffers);
	printf("scheduling algorithm: %s", schedalg);

	buffer_size = buffers;
	buffer = malloc(sizeof(int) * buffer_size);

    // run out of this directory
    chdir_or_die(root_dir);

	// Initialize bounded buffer
    buffer_init(buffer_size);

	// Start worker threads
	pthread_t tid[threads];
	for (int i = 0; i < threads; i++){
		pthread_create(&tid[i], NULL, worker, NULL);
	}

    // Master thread: accept connections and put them into the buffer
    int listen_fd = open_listen_fd_or_die(port);
    while (1) {
		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
		printf("Accepted connection %d\n", conn_fd);

		struct stat sbuf;
		int fs = request_get_file_size(conn_fd);
		printf("File size: %d\n", fs);
		request_handle(conn_fd);
		close_or_die(conn_fd);
    }
    return 0;
}
