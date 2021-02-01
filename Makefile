proj := bittyhttp

src := $(wildcard src/*.c)
src := $(filter-out src/http_parser.c,$(src))
src := $(filter-out src/main.c,$(src))
obj := $(src:.c=.o)
CC := gcc
# CFLAGS := -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -std=c99 -O3
CFLAGS := -D_GNU_SOURCE -D_POSIX_C_SOURCE=200112L -std=c99 -O3
CWARN := -Wall
INC := -lpthread -lcurl

all: $(proj)

libbhttp.a: $(obj) http_parser.o
	ar rcs libbhttp.a $^
	rm -f $^

bittyhttp: main.o $(obj) http_parser.o
	$(CC) -o $@ $(CFLAGS) $^ $(INC)
	rm -f main.o
	rm -f $(obj)
	rm -f http_parser.o

main.o:
	$(CC) $(CFLAGS) -o $@ -c src/main.c

%.o: %.c
	$(CC) $(CWARN) $(CFLAGS) -o $@ -c $<

http_parser.o:
	$(CC) $(CFLAGS) -o $@ -c src/http_parser.c

.PHONY: clean
clean:
	rm -f $(obj) $(proj)
	rm -f http_parser.o

install:
	sudo cp bittyhttp /usr/local/bin/
	sudo cp bittyhttp.service /etc/systemd/system/
	sudo systemctl enable bittyhttp.service
	sudo systemctl start bittyhttp.service
