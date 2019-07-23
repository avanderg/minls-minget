# Name: Denis Pyryev (dpyryev)
#
# Date: May 5, 2019
# Assignment: Program 5
# Instructor: Professor Phillip Nico
# Course: CPE 453-03
#
# Description: Makefile for Program5 (minls and minget)

CC 	= gcc

CFLAGS   = -Wall -g -fpic 

EXTRACLEAN = minls minget

all: minls minget
	

minls: minls.o utilities.o
	$(CC) $(CFLAGS) -o minls minls.o utilities.o utilities.h

minls.o: minls.c
	$(CC) $(CFLAGS) -c minls.c

minget: minget.o utilities.o
	$(CC) $(CFLAGS) -o minget minget.o utilities.o utilities.h 

minget.o: minget.c
	$(CC) $(CFLAGS) -c minget.c

utilities.o: utilities.c
	$(CC) $(CFLAGS) -c utilities.c

clean:
	@rm -f *.o $(EXTRACLEAN)
