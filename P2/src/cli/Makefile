CC = gcc

# Para mais informações sobre as flags de warning, consulte a informação adicional no lab_ferramentas
CFLAGS =  -g -std=c17 -D_POSIX_C_SOURCE=200809L \
		  -Wall -Werror -Wextra \
		  -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused
#CFLAGS += -fsanitize=address 
#CFLAGS += -fsanitize=undefined 
#CFLAGS += -fsanitize=thread

DIR_PATH = ../jobs

TEST_DIR_PATH = ../j_test

.PHONY = run clean full all test

ifneq ($(shell uname -s),Darwin) # if not MacOS
	CFLAGS += -fmax-errors=5
endif

all: client

client: client.c constants.h parser.o api.o
	$(CC) $(CFLAGS) -o client client.c parser.o api.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c ${@:.o=.c}

clean:
	rm -f *.o $(DIR_PATH)/*.myout $(TEST_DIR_PATH)/*.myout client

full: clean all