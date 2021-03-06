CC=gcc
CFLAGS=-std=c99 -ftrapv -pthread

all: server client

server: server.o ftransfer.o handshake.o helper.o util.o
	$(CC) $(CFLAGS) server.o ftransfer.o handshake.o helper.o util.o -o server

client: client.o ftransfer.o handshake.o helper.o util.o
	$(CC) $(CFLAGS) client.o ftransfer.o handshake.o helper.o util.o -o client

server.o: server.c
	$(CC) $(CFLAGS) server.c -c

client.o: client.c
	$(CC) $(CFLAGS) client.c -c

ftransfer.o: ftransfer.c
	$(CC) $(CFLAGS) ftransfer.c -c

handshake.o: handshake.c
	$(CC) $(CFLAGS) handshake.c -c

helper.o: helper.c
	$(CC) $(CFLAGS) helper.c -c

util.o: util.c
	$(CC) $(CFLAGS) -c util.c

clean:
	rm -rf *.o server client received_file cs118-zhanyangli.tar.gz

tar:
	tar czvf cs118-zhanyangli.tar.gz *.c *.h Makefile 