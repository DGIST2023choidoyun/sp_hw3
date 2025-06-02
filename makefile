CFLAGS = -g -O0
LDFLAGS = -ljansson

.PHONY: all clean

all: client server

client: client.clean
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

server: server.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f client server
