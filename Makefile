CARGS=-W -Wall -g -O0
all:
	gcc $(CARGS) -o convert dbf2cvs.c
