CC=gcc
CFLAGS=

all: server client

server: server.o util.o
	$(CC) $(CFLAGS) server.o util.o -o server

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

util.o: util.c
	$(CC) $(CFLAGS) -c util.c

client: client.o client-helper.o util.o
	$(CC) $(CFLAGS) client.o client-helper.o util.o -o client

client.o: client.c
	$(CC) $(CFLAGS) client.c -c

client-helper.o: client-helper.c
	$(CC) $(CFLAGS) client-helper.c -c

clean:
	rm -rf *.o server client