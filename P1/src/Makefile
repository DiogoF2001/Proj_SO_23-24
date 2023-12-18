CC = gcc

# Para mais informações sobre as flags de warning, consulte a informação adicional no lab_ferramentas
CFLAGS =  -g -std=c17 -D_POSIX_C_SOURCE=200809L \
		  -Wall -Werror -Wextra \
		  -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused
#CFLAGS += -fsanitize=address 
#CFLAGS += -fsanitize=undefined 
#CFLAGS += -fsanitize=thread

DIR_PATH = ../jobs
MAX_PROC = 4
MAX_THREADS = 10

TEST_DIR_PATH = ../j_test

.PHONY = run clean full all test

ifneq ($(shell uname -s),Darwin) # if not MacOS
	CFLAGS += -fmax-errors=5
endif

all: ems

ems: main.c constants.h operations.o parser.o eventlist.o
	$(CC) $(CFLAGS) $(SLEEP) -o ems main.c operations.o parser.o eventlist.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c ${@:.o=.c}

run: ems
	@./ems $(DIR_PATH) $(MAX_PROC) $(MAX_THREADS)

clean:
	rm -f *.o $(DIR_PATH)/*.myout $(TEST_DIR_PATH)/*.myout ems

full: clean all run

test: ems
	@./ems $(TEST_DIR_PATH) $(MAX_PROC) $(MAX_THREADS)