#include <stdio.h>
#include "request.h"
#include "io_helper.h"
#include "bounded_buffer.h"
#include "request_info.h"
#include <pthread.h>

char default_root[] = "www";

int buffer_size;

// Worker process
void *worker(void *arg) {
	while (1) {
		// get a connection from buffer. this is threadsafe.
		request_info_t req = buffer_get();
		printf("[WORKER] Starting request: %d, file: %s (size=%ld), cgiargs: %s\n", req.fd, req.filename, req.sbuf.st_size, req.cgiargs);
		// handle HTTP request
		request_handle(&req);
		// close the connection
		close_or_die(req.fd);
		printf("[WORKER] Complete request: %d, file: %s (size=%ld), cgiargs: %s\n", req.fd, req.filename, req.sbuf.st_size, req.cgiargs);
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

	// Process command-line options until getopt returns -1 (no more options)
    while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
		switch (c) {
			case 'd': // directory
	    		root_dir = optarg;
	    		break;
			case 'p': // port
	    		port = atoi(optarg);
	    		break;
			case 't': // threads
				threads = atoi(optarg);
				if (buffers <= 0){
					fprintf(stderr, "Error: Invalid number of threads '%s'. threads must be a positive integer\n", optarg);
					fprintf(stderr, "usage: wserver [-d baseddir] [-p port] [-t threads] [-b buffersize] [-s policy]\n");
				}
				break;
			case 'b': // buffer size
				buffers = atoi(optarg);
				if (buffers <= 0){
					fprintf(stderr, "Error: Invalid buffer size '%s'. buffer must be a positive integer\n", optarg);
					fprintf(stderr, "usage: wserver [-d baseddir] [-p port] [-t threads] [-b buffersize] [-s policy]\n");
				}
				break;
			case 's': // scheduling algorithm
				if (strcmp(optarg, "FIFO") == 0 || strcmp(optarg, "SFF") == 0){
					schedalg = optarg;
				} else {
					fprintf(stderr, "Error: Invalid schedulign algorithm '%s'. Must be either FIFO or SFF.\n", optarg);
					fprintf(stderr, "usage: wserver [-d baseddir] [-p port] [-t threads] [-b buffersize] [-s policy]\n");
					exit(1);
				}
				schedalg = optarg;
				break;
			default: // errors
	    		fprintf(stderr, "usage: wserver [-d baseddir] [-p port] [-t threads] [-b buffersize] [-s policy]\n");
	    		exit(1);
		}
	//printf("num threads: %d", threads);
	//printf("buffer size: %d", buffers);
	//printf("scheduling algorithm: %s", schedalg);

	buffer_size = buffers;
	// set the SSF flag
	int sff_flag = strcmp(schedalg, "SFF") == 0;

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
		printf("======================\n");
		printf("Accepted connection %d\n", conn_fd);

		request_info_t req;
		// get info about the user's request
		int success = request_get_info(conn_fd, &req);//, root_dir);
		// if successful, handle request.
		if (success == 0) {
			printf("\tmethod: %s\n\turi: %s\n\tversion: %s\n", req.method, req.uri, req.version);
			// put the request data in the bounded buffer. ssf_flag determines whether or not the buffer will be sorted
			buffer_put(req, sff_flag);
			printf("Request %d added to buffer\n", conn_fd);
		}
		else {
			// close connection
			close_or_die(conn_fd);
		}
    }
    return 0;
}
