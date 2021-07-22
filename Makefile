#
# Makefile for compiling mimelang
#
# Author: Werner Fink,  <werner@suse.de>
#

MAJOR    :=     0
MINOR    :=     1
VERSION  :=     $(MAJOR).$(MINOR)

   CC := gcc -g3
CFLAGS = $(RPM_OPT_FLAGS) -Wall -pipe -funroll-loops
    RM = rm -f


TODO = mimelang

all: $(TODO)

mimelang: mimelang.c blocks.o
	$(CC) $(CFLAGS) -o $@ $< blocks.o

clean:
	$(RM) *.o *.a *.so* *~ $(TODO)
