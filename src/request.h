#ifndef __REQUEST_H__
#include "request_info.h"
void request_handle(request_info_t *r);

// returns 0 on success, -1 on failure.
int request_get_info(int fd, request_info_t *request_info_out);

#endif
