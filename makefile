all:
	gcc -Wall -c common.c
	gcc -Wall client.c common.o -lpthread -o client
	gcc -Wall server.c common.o -lpthread -o server