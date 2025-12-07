#ifndef REQUEST_INFO_H
#define REQUEST_INFO_H
	#include "common.h"
	#include <sys/stat.h>
	typedef struct {
		int fd;
		struct stat sbuf;
		//int file_size;
		int is_static;
		char method[MAXBUF];
		char uri[MAXBUF];
		char version[MAXBUF];
		char filename[MAXBUF];
		char cgiargs[MAXBUF];
	} request_info_t;

#endif
