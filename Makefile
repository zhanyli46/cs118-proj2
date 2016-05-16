CC=gcc
CFLAGS=

all: server client

server: server.o util.o
	$(CC) $(CFLAGS) server.o util.o -o server

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

util.o: util.c
	$(CC) $(CFLAGS) -c util.c

client: client.o helper.o util.o
	$(CC) $(CFLAGS) client.o helper.o util.o -o client

client.o: client.c
	$(CC) $(CFLAGS) client.c -c

client-helper.o: helper.c
	$(CC) $(CFLAGS) helper.c -c

clean:
	rm -rf *.o server client