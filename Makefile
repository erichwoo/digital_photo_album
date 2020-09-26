#Makefile for "album.c"

# Uncomment VERBOSE for step by step print statements
#    detailing the flow and creation of processes
# Uncomment WAIT to see how each individual img process
#    waits for the others. WAIT is a subset of VERBOSE

#VERBOSE = -DVERBOSE  
#WAIT = -DWAIT

CC = gcc
CFLAGS = -Wall -pedantic -std=c11 -ggdb $(VERBOSE) $(WAIT)
PROG = album
OBJS = $(PROG).o demo.o

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

album.o: demo.h

.PHONY: clean

clean:	
	rm -rf index.html album *.jpg .*~ *~ *.o *.dSYM core
