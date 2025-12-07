#ifndef BOUNDED_BUFFER_H
#define BOUNDED_BUFFER_H
#include "request_info.h"

void buffer_init(int size);
void buffer_put(request_info_t item, int sff_flag);
request_info_t buffer_get();

#endif
