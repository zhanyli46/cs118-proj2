CC=gcc
CFLAGS=

all: server client

server: server.o helper.o util.o
	$(CC) $(CFLAGS) server.o helper.o util.o -o server

client: client.o helper.o util.o
	$(CC) $(CFLAGS) client.o helper.o util.o -o client

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

client.o: client.c
	$(CC) $(CFLAGS) client.c -c

helper.o: helper.c
	$(CC) $(CFLAGS) helper.c -c

util.o: util.c
	$(CC) $(CFLAGS) -c util.c

clean:
	rm -rf *.o server client