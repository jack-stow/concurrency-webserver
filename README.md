
# Compilation

1. Open a terminal and navigate to the src/ directory:
   ```sh
    cd src
    ```
1. Build the server, client, and CGI programs using the provided Makefile:
   ```sh
    cd src
    ```
This will produce:
- **wserver**: The webserver
- **wclient**: A simple HTTP client for testing
- **spin.cgi**: The dynamic CGI program (outputed to the www directory for convenience)
- All required **.o** files



# Command-line Parameters

The web server is invoked as follows:

```sh
./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]
```

- **basedir**: This is the root directory from which the web server will operate. 
  The server will try to ensure that file accesses does not access files above 
  this directory in the file-system hierarchy. Default: ./www
- **port**: The port number that the web server should listen on.
  Default: 10000.
- **threads**: The number of worker threads that should be created within the web
  server. Must be a positive integer. Default: 1.
- **buffers**: The number of request connections that can be accepted at one
  time. Must be a positive integer. Note that it is not an error for more or
  less threads to be created than buffers. Default: 1.
- **schedalg**: the scheduling algorithm to be performed. Must be one of FIFO
  or SFF. Default: FIFO.

For example, you could run your program as:
```sh
./wserver -d www -p 8000 -t 4 -b 8 -s SFF
```

In this case, your web server will listen to port 8000, create 4 worker threads for
handling HTTP requests, allocate 8 buffers for connections that are currently
in progress (or waiting), and use SFF scheduling for arriving requests.

# Testing Using the Client
You can send requests using the provided client:

```sh
./wclient <host> <port> <filepath>
```

Examples:
```sh
./wclient localhost 8000 index.html
./wclient localhost 8000 spin.cgi?2
```

# Source Code Overview


## Scheduling Policies

The scheduling policy is determined by a command line argument when the web
server is started and are as follows:

- **First-in-First-out (FIFO)**: When a worker thread wakes, it handles the
first request (i.e., the oldest request) in the buffer. Note that the HTTP
requests will not necessarily finish in FIFO order; the order in which the
requests complete will depend upon how the OS schedules the active threads.

- ** Smallest File First (SFF)**: When a worker thread wakes, it handles the
request for the smallest file. This policy approximates Shortest Job First to
the extent that the size of the file is a good prediction of how long it takes
to service that request. Requests for static and dynamic content may be
intermixed, depending upon the sizes of those files. Note that this algorithm
can lead to the starvation of requests for large files.  You will also note
that the SFF policy requires that something be known about each request (e.g.,
the size of the file) before the requests can be scheduled. Thus, to support
this scheduling policy, you will need to do some initial processing of the
request (hint: using `stat()` on the filename) outside of the worker threads;
you will probably want the master thread to perform this work, which requires
that it read from the network descriptor.
