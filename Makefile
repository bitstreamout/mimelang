#
# Makefile for compiling mimelang
#
# Author: Werner Fink,  <werner@suse.de>
#

MAJOR    :=     0
MINOR    :=     2
VERSION  :=     $(MAJOR).$(MINOR)

   CC := gcc -g3
CFLAGS = $(RPM_OPT_FLAGS) -Wall -pipe -funroll-loops
    RM = rm -f


TODO = mimelang

all: $(TODO)

blocks.o: mimelang.h
decode.o: mimelang.h

mimelang: mimelang.c blocks.o decode.o mimelang.h
	$(CC) $(CFLAGS) -o $@ $< blocks.o decode.o

clean:
	$(RM) *.o *.a *.so* *~ $(TODO)
