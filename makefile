C = gcc
CFLAGS= -std=gnu99 -Wall -Wpedantic -pedantic -g
CFLAGS2= -o

client:
	$(C) $(CFLAGS) client.c ${CFLAGS2} client.x

server:
	$(C) $(CFLAGS) server.c ${CFLAGS2} server.x

clean:
	rm -v *.x