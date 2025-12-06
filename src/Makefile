# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -pthread
OBJS = wserver.o wclient.o request.o io_helper.o bounded_buffer.o

# Targets
all: wserver wclient spin.cgi

wserver: wserver.o request.o io_helper.o bounded_buffer.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

wclient: wclient.o io_helper.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

spin.cgi: spin.c
	$(CC) $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f *.o wserver wclient spin.cgi

debug: CFLAGS += -g -DDEBUG
debug: clean all
	@echo "Built everything with debug symbols"
