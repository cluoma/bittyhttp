proj := bittyhttp

CC := gcc
CFLAGS := -D_GNU_SOURCE -D_POSIX_C_SOURCE=200112L -std=c99 -O3
CWARN := -Wall

SRCS := $(wildcard src/*.c)
SRCS := $(filter-out src/http_parser.c,$(SRCS))
OBJS := $(SRCS:.c=.o)

EX_SRCS := examples/examples.c
EX_OBJS := $(EX_SRCS:.c=.o)
EX_INCL := -lpthread -lcurl

all:

lib: libbhttp.a

example: $(EX_OBJS) libbhttp.a
	$(CC) -o $@ $(CFLAGS) $(EX_OBJS) -lbhttp $(EX_INCL) -L.
	rm -f $^

libbhttp.a: $(OBJS) http_parser.o
	ar rcs libbhttp.a $^
	rm -f $^

main.o:
	$(CC) $(CFLAGS) -o $@ -c src/main.c

%.o: %.c
	$(CC) $(CWARN) $(CFLAGS) -o $@ -c $<

http_parser.o:
	$(CC) $(CFLAGS) -o $@ -c src/http_parser.c

.PHONY: clean
clean:
	rm -f $(OBJS)
	rm -f http_parser.o
