#ifndef BOUNDED_BUFFER_H
#define BOUNDED_BUFFER_H

void buffer_init(int size);
void buffer_put(int fd, int file_size);
int buffer_get(int *file_size_out);

#endif
