#include "io_helper.h"
#include "request.h"
#include "common.h"
#include "request_info.h"
//
// Some of this code stolen from Bryant/O'Halloran
// Hopefully this is not a problem ... :)
//

//#define MAXBUF (8192)

void request_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXBUF], body[MAXBUF];

    // Create the body of error message first (have to know its length for header)
    sprintf(body, ""
	    "<!doctype html>\r\n"
	    "<head>\r\n"
	    "  <title>OSTEP WebServer Error</title>\r\n"
	    "</head>\r\n"
	    "<body>\r\n"
	    "  <h2>%s: %s</h2>\r\n"
	    "  <p>%s: %s</p>\r\n"
	    "</body>\r\n"
	    "</html>\r\n", errnum, shortmsg, longmsg, cause);

    // Write out the header information for this response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Content-Type: text/html\r\n");
    write_or_die(fd, buf, strlen(buf));

    sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
    write_or_die(fd, buf, strlen(buf));

    // Write out the body last
    write_or_die(fd, body, strlen(body));
}

//
// Reads and discards everything up to an empty text line
//
void request_read_headers(int fd) {
    char buf[MAXBUF];

    readline_or_die(fd, buf, MAXBUF);
    while (strcmp(buf, "\r\n")) {
		readline_or_die(fd, buf, MAXBUF);
    }
    return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
//For static requests:
// - Clears cgiargs
// - Converts the URI into a local filename (stripping a leading '/' if present).
// - If the URI ends with '/', appends "index.html" to serve a default page.
//
// For Dynamic requests (URIs containing "cgi"):
// - Splits the URI at the '?' if present, storing everything after it in cgiargs.
// - Removes the query portion form the URI so the remaining part becomes the filename.
// - Strips a leading '/' from the filename if needed.
//
// The computed filename is placed in 'filename', and any query arguments go in 'cgiargs'.
int request_parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;

    if (!strstr(uri, "cgi")) {
		// static
		strcpy(cgiargs, "");
		//sprintf(filename, ".%s", uri);
		if (uri[0] == '/') {
	    	sprintf(filename, "%s", uri + 1);
		}
		else{
			sprintf(filename, "%s", uri);
		}
		if (uri[strlen(uri) - 1] == '/'){
			strcat(filename, "index.html");
		}
		return 1;
    } else {
		// dynamic
		ptr = index(uri, '?');
		if (ptr) {
	    	strcpy(cgiargs, ptr+1);
	    	*ptr = '\0';
		} else {
	    	strcpy(cgiargs, "");
		}
		if (uri[0] == '/'){
			sprintf(filename, "%s", uri + 1);
		}
		else{
			sprintf(filename, "%s", uri);
		}
		return 0;
    }
}

//
// Fills in the filetype given the filename
//
void request_get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
    else
		strcpy(filetype, "text/plain");
}

void request_serve_dynamic(int fd, char *filename, char *cgiargs) {
    char buf[MAXBUF], *argv[] = { NULL };

    // The server does only a little bit of the header.
    // The CGI script has to finish writing out the header.
    sprintf(buf, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: OSTEP WebServer\r\n");

    write_or_die(fd, buf, strlen(buf));

	if (fork_or_die() == 0) {                        // child
		setenv_or_die("QUERY_STRING", cgiargs, 1);   // args to cgi go here
		dup2_or_die(fd, STDOUT_FILENO);              // make cgi writes go to socket (not screen)
		extern char **environ;                       // defined by libc
		execve_or_die(filename, argv, environ);
    } else {
		wait_or_die(NULL);
    }
}

void request_serve_static(int fd, char *filename, int filesize) {
    int srcfd;
    char *srcp, filetype[MAXBUF], buf[MAXBUF];

    request_get_filetype(filename, filetype);
    srcfd = open_or_die(filename, O_RDONLY, 0);

    // Rather than call read() to read the file into memory,
    // which would require that we allocate a buffer, we memory-map the file
    srcp = mmap_or_die(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close_or_die(srcfd);

    // put together response
    sprintf(buf, ""
	    "HTTP/1.0 200 OK\r\n"
	    "Server: OSTEP WebServer\r\n"
	    "Content-Length: %d\r\n"
	    "Content-Type: %s\r\n\r\n",
	    filesize, filetype);

    write_or_die(fd, buf, strlen(buf));

    //  Writes out to the client socket the memory-mapped file
    write_or_die(fd, srcp, filesize);
    munmap_or_die(srcp, filesize);
}

// Parses the incoming HTTP request and fills a request_info_t struct for later
// handling. Returns 0 on success or -1 if the request is invalid.
//
// Steps performed:
// 1. Read the request line and extract method, URI, and version. Only GET is allowed.
// 2. Reject attempts to use parent-directory traversal ("..").
// 3. Read and discard remaining headers.
// 4. Use request_parse_uri() to determine whether the request is static or CGI,
//    and to compute the target filename and any query arguments.
// 5. Resolve the requested file against the server's current working directory,
//    canonicalizing it with realpath().
// 6. Ensure the resolved path stays in the server's root directory, blocking attempts to escape the tree
// 7. stat() the resolved file and report errors if it does not exist.
// 8. Populate the output request_info_t structure with all parsed and resolved data.
int request_get_info(int fd, request_info_t *request_info_out){
	int is_static;
    struct stat sbuf;
    char buf[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
    char filename[MAXBUF], cgiargs[MAXBUF];

    readline_or_die(fd, buf, MAXBUF);
    sscanf(buf, "%s %s %s", method, uri, version);
    printf("method: %s\nuri: %s\nversion: %s\n", method, uri, version);

	// Only GET requests are supported
    if (strcasecmp(method, "GET")) {
		request_error(fd, method, "501", "Not Implemented", "server does not implement this method");
		return -1;
    }

	// ensure the user doesn't attempt to traverse up the file tree
	if (strstr(uri, "..") != NULL) {
		request_error(fd, uri, "403", "Forbidden", "parent directory references are not allowed");
		return -1;
	}

	// read remaining headers
    request_read_headers(fd);

	// determine filename and CGI args
    is_static = request_parse_uri(uri, filename, cgiargs);
    //printf("[DEBUG] URI from client: '%s'\n", uri);
    //printf("[DEBUG] Filename after request_parse_uri: '%s'\n", filename);

	// Make sure file is inside root_dir
	char abs_root[MAXBUF];
	if (getcwd(abs_root, MAXBUF) == NULL) {
		//printf("[DEBUG] cwd - abs_root = '%s'\n", abs_root);
		perror("getcwd");
		return -1;
	}
	//printf("[DEBUG] Absolute abs_root: '%s'\n", abs_root);

	// Construct full path and canonicalize
	char fullpath[MAXBUF];
	snprintf(fullpath, MAXBUF, "%s/%s", abs_root, filename);
	//printf("[DEBUG] **Fullpath (root_dir + filename): '%s'\n", fullpath);

	// Canonicalize requested file
	char resolved[MAXBUF];
	if (realpath(fullpath, resolved) == NULL) {
		perror("realpath");
		//printf("[DEBUG] resolved = '%s'\n", resolved);
		request_error(fd, uri, "404", "Not Found", "file not found");
		return -1;
	}
	//printf("[DEBUG] Resolved absolute path: '%s'\n", resolved);

	// Ensure file is inside root_dir
	if (strncmp(resolved, abs_root, strlen(abs_root)) != 0) {
		//printf("[DEBUG] Access outside base directory: '%s'\n", resolved);
		request_error(fd, uri, "403", "Forbidden", "access outside base directory");
		return -1;
	}

	// stat the resolved file
    if (stat(resolved, &sbuf) < 0) {
		request_error(fd, resolved, "404", "Not found", "server could not find this file");
		return -1;
    }

	//printf("[DEBUG] File to serve (stored in request_info): '%s'\n", resolved);
	// Fill request_info struct
    request_info_out->fd = fd;
    request_info_out->sbuf = sbuf;
    request_info_out->is_static = is_static;
	strcpy(request_info_out->method, method);
	strcpy(request_info_out->uri, uri);
	strcpy(request_info_out->version, version);
	strcpy(request_info_out->filename, resolved);
	strcpy(request_info_out->cgiargs, cgiargs);

    printf("Looking for file: %s\n", filename);
    printf("resolved filename: %s\n", resolved);
	return 0;
}

// Processes a prepared request described by request_info_t. Chooses between
// serving static files or executing a CGI program based on is_static.
//
// For static content:
//  - Ensures the file exists, is a regular file, and is readable
//  - Sends it using request_serve_static().
//
// For dynamic content (CGI):
//  - Ensures the file exists, is a regular file, and is executable.
//  - Runs it using request_serve_dynamic(), passing any CGI arguments.
//
// Any access or permission issue results in an appropriate HTTP error response
void request_handle(request_info_t *r) { // int fd
	printf("[WORKER] handling request: %d, file: %s (size=%ld)\n", r->fd, r->filename, r->sbuf.st_size);
    if (r->is_static) {
		if (!(S_ISREG(r->sbuf.st_mode)) || !(S_IRUSR & r->sbuf.st_mode)) {
	    	request_error(r->fd, r->filename, "403", "Forbidden", "server could not read this file");
	    	return;
		}
		request_serve_static(r->fd, r->filename, r->sbuf.st_size);
    } else {
		if (!(S_ISREG(r->sbuf.st_mode)) || !(S_IXUSR & r->sbuf.st_mode)) {
	    	request_error(r->fd, r->filename, "403", "Forbidden", "server could not run this CGI program");
	    	return;
		}
		request_serve_dynamic(r->fd, r->filename, r->cgiargs);
    }
}
