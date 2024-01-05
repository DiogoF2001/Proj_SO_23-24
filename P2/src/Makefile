CC = gcc

CFLAGS =  -g -std=c17 -D_POSIX_C_SOURCE=200809L \
		  -Wall -Werror -Wextra \
		  -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused
#CFLAGS += -fsanitize=address 
#CFLAGS += -fsanitize=undefined 
#CFLAGS += -fsanitize=thread

DIR_PATH = jobs

.PHONY = clean full all

ifneq ($(shell uname -s),Darwin) # if not MacOS
	CFLAGS += -fmax-errors=5
endif

all: client server

client: cli/client.c cli/constants.h cli/parser.o cli/api.o
	$(CC) $(CFLAGS) -o client cli/client.c parser.o api.o

server: serv/server.c serv/constants.h serv/operations.o serv/pipe_parser.o serv/eventlist.o
	$(CC) $(CFLAGS) -o server serv/server.c operations.o pipe_parser.o eventlist.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c ${@:.o=.c}

clean:
	rm -f ./*.o $(DIR_PATH)/*.out client server

full: clean all