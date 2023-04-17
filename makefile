C = gcc
CFLAGS= -o

client:
	$(C) $(CFLAGS) client.x client.c

server:
	$(C) $(CFLAGS) server.x server.c

clean:
	rm -v *.x