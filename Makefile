CC=gcc
CFLAGS=-c -Wall -g

all: battle_server battle_client


battle_server.o: battle_server.c 
	$(CC) $(CFLAGS) battle_server.c

battle_client.o: battle_client.c
	$(CC) $(CFLAGS) battle_client.c

battle_server: battle_server.o
	$(CC) battle_server.o -o battle_server

battle_client: battle_client.o
	$(CC) battle_client.o -o battle_client 

clean:
	rm *.o battle_server battle_client
