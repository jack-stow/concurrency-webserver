# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -pthread
OBJS = wserver.o wclient.o request.o io_helper.o bounded_buffer.o

CGI_DIR = www

# Targets
all: wserver wclient $(CGI_DIR)/spin.cgi

wserver: wserver.o request.o io_helper.o bounded_buffer.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

wclient: wclient.o io_helper.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(CGI_DIR)/spin.cgi: spin.c | $(CGI_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(CGI_DIR):
	mkdir -p $(CGI_DIR)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f *.o wserver wclient $(CGI_DIR)/spin.cgi

debug: CFLAGS += -g -DDEBUG
debug: clean all
	@echo "Built everything with debug symbols"
