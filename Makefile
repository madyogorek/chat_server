#Makefile to make the file hehe lol
#to use, type
#	> make
#Which will create the program.
include test_Makefile
#To create and run the test program use
#	> make test

CFLAGS	=	-Wall	-g											#variable holding options to the c compiler
CC	=	gcc	$(CFLAGS)										#variable holding the compilation command




bl_server	:	bl_server.o	server_funcs.o	util.o
	$(CC)	-o	bl_server	bl_server.o	server_funcs.o	util.o	-lpthread

bl_server.o	:	bl_server.c	blather.h	#commando.o depends on 2 source files
	$(CC)	-c	bl_server.c						#compile only, don't link yet

server_funcs.o	:	server_funcs.c	blather.h
	$(CC)	-c	server_funcs.c

bl_client.o	:	bl_client.c	simpio.c blather.h
	$(CC)	-c	bl_client.c	simpio.c	-lpthread

util.o	:	util.c	blather.h
	$(CC)	-c	util.c

simpio.o	:	simpio.c	blather.h
	$(CC)	-c	simpio.c

bl_client	:	bl_client	bl_client.o	simpio.o	util.o
	$(CC)	-o	bl_client	bl_client.o	simpio.o	util.o	-lpthread


clean:
	@echo	Cleaning up object files
	rm	-f	*.o	bl_server
	rm	-f	*.o	bl_client
