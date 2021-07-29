#
# Makefile for compiling mimelang
#
# Author: Werner Fink,  <werner@suse.de>
#

MAJOR    :=     0
MINOR    :=     2
VERSION  :=     $(MAJOR).$(MINOR)
prefix    =     /usr
bindir    =     $(prefix)/bin
mandir    =     $(prefix)/share/man
man1dir   =     $(mandir)/man1

    CC := gcc -g3
 CFLAGS = $(RPM_OPT_FLAGS) -Wall -pipe -funroll-loops
     RM = rm -f
INSTALL = install
MKDIRS  = mkdir -p

   TODO = mimelang

all: $(TODO)
install: install-exec install-man

blocks.o: mimelang.h
decode.o: mimelang.h

mimelang: mimelang.c blocks.o decode.o mimelang.h
	$(CC) $(CFLAGS) -o $@ $< blocks.o decode.o

clean:
	$(RM) *.o *.a *.so* *~ $(TODO)

install-exec: mimelang
	$(MKDIRS) $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 $< $(DESTDIR)$(bindir)/

install-man: mimelang.1
	$(MKDIRS) $(DESTDIR)$(man1dir)
	$(INSTALL) -m 644 $< $(DESTDIR)$(man1dir)/
