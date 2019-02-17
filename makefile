#kompiluje się po prostu komendą make

all: program1 program2

program1: server.c
	gcc -Wall -pthread server.c -o server

program2: client.c
	gcc -Wall -pthread client.c -o client
