CC     = gcc
CFLAGS = -O2 -Wall
INCDIR = -I/usr/X11R6/include/X11
DESTDIR= /usr/local
LIBDIR = -L/usr/X11R6/lib
# for linux
LIBS   = -lXpm -lX11 -lXext
# for Solaris
#LIBS   = -lXpm -lX11 -lXext -lsocket
OBJS   = wmMand.o \
         ../wmgeneral/wmgeneral.o xvgifwr.o


.c.o:
	$(CC) $(CFLAGS) -D$(shell echo `uname -s`) -c $< -o $*.o $(INCDIR)


all:	wmMand.o wmMand

wmMand.o: wmMand_master.xpm wmMand_mask.xbm
wmMand:	$(OBJS) 
	$(CC) $(CFLAGS) $(SYSTEM) -o wmMand $(OBJS) $(INCDIR) $(LIBDIR) $(LIBS)

clean:
	for i in $(OBJS) ; do \
		rm -f $$i; \
	done
	rm -f wmMand

install:: wmMand
	install -d $(DESTDIR)
	install -d $(DESTDIR)/man/man6
	install -d $(DESTDIR)/share/pixmaps
	install -s -m 0755 wmMand $(DESTDIR)/bin
	install    -m 0644 wmMand.6.gz $(DESTDIR)/man/man6
	install	   -m 0644 wmMand.png $(DESTDIR)/share/pixmaps
