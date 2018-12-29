#
#
# Author: Teunis van Beelen
#
# email: teuniz@gmail.com
#
#

CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wshadow -Wformat-nonliteral -Wformat-security -Wtype-limits -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE
LDFLAGS =
LDLIBS = -lm

objects = main.o edflib.o

all: edf_generator

edf_generator : $(objects)
	$(CC) $(LDLIBS) $(objects) -o edf_generator

main.o : main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

edflib.o : edflib.h edflib.c
	$(CC) $(CFLAGS) -c edflib.c -o edflib.o

clean :
	$(RM) edf_generator $(objects)

#
#
#
#




