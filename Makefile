proj := bittyhttp

CC 			:= gcc
CFLAGS 		:= -D_POSIX_C_SOURCE=200809L -std=c99 -O3
CWARN 		:= -Wall
DEFINES 	:=
INCL		:=

SRCS := $(wildcard src/*.c)
SRCS := $(filter-out src/http_parser.c,$(SRCS))
OBJS := $(SRCS:.c=.o)

EX_SRCS := examples/examples.c
EX_OBJS := $(EX_SRCS:.c=.o)
EX_LIBS := -lpthread -lcurl

all: example

lib: libbhttp.a

debug: CFLAGS += -g -O1
debug: example

luasqliteexample: examples/lua_sqlite_example.o libbhttp.a
	$(CC) -o $@ $(CFLAGS) examples/lua_sqlite_example.o -lbhttp $(EX_LIBS) -L.
	rm -f $^

example: $(EX_OBJS) libbhttp.a
	$(CC) -o $@ $(CFLAGS) $(EX_OBJS) -lbhttp $(EX_LIBS) -L.
	rm -f $^

libbhttp.a: $(OBJS) http_parser.o
	ar rcs libbhttp.a $^
	rm -f $^

main.o:
	$(CC) $(CFLAGS) -o $@ -c src/main.c

%.o: %.c
	$(CC) $(CWARN) $(CFLAGS) $(DEFINES) $(INCL) -o $@ -c $<

http_parser.o:
	$(CC) $(CFLAGS) -o $@ -c src/http_parser.c

.PHONY: clean
clean:
	rm -f $(OBJS)
	rm -f http_parser.o
