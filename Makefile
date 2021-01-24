proj := bittyhttp

src := $(wildcard src/*.c)
src := $(filter-out src/http_parser.c,$(src))
obj := $(src:.c=.o)
CC := clang
CFLAGS := -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -std=c99 -O3
CWARN := -Wall
INC := -lpthread

all: $(proj)

bittyhttp: $(obj) http_parser.o
	$(CC) -o $@ $(CFLAGS) $^ -lpthread
	rm -f $(obj)
	rm -f http_parser.o

%.o: %.c
	$(CC) $(CWARN) $(CFLAGS) -o $@ -c $<

http_parser.o:
	$(CC) $(CFLAGS) -o $@ -c src/http_parser.c

.PHONY: clean
clean:
	rm -f $(obj) $(proj)
	rm -f http_parser.o

install:
	sudo cp MiniHTTP /usr/local/bin/
	sudo cp minihttp.service /etc/systemd/system/
	sudo systemctl enable minihttp.service
	sudo systemctl start minihttp.service
