#
#
# Author: Teunis van Beelen
#
# email: teuniz@protonmail.com
#
#

CC = gcc
CFLAGS = -O2 -std=gnu11 -Wall -Wextra -Wshadow -Wformat-nonliteral -Wformat-security -Wtype-limits -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE
LDFLAGS =
LDLIBS = -lm

objects = obj/main.o obj/edflib.o obj/utils.o
headers = utils.h edflib.h

all: edfgenerator

edfgenerator : $(objects)
	$(CC) $(objects) -o edfgenerator $(LDLIBS)

obj/main.o : main.c $(headers)
	$(CC) $(CFLAGS) -c main.c -o obj/main.o

obj/edflib.o : edflib.c $(headers)
	$(CC) $(CFLAGS) -c edflib.c -o obj/edflib.o

obj/utils.o : utils.c $(headers)
	$(CC) $(CFLAGS) -c utils.c -o obj/utils.o

clean :
	$(RM) edfgenerator $(objects)

#
#
#
#





