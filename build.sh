#!/bin/sh
#
gcc main.c edflib.c -lm -s -O2 -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -Wall -W -Wextra -Wshadow -Wformat-nonliteral -Wformat-security -Wtype-limits -Wfatal-errors -o edf_generator
#
#gcc main.c edflib.c -lm -g -O0 -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -Wall -W -Wextra -Wshadow -Wformat-nonliteral -Wformat-security -Wtype-limits -Wfatal-errors -o edf_generator
#