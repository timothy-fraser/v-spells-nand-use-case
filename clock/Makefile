
LIBDIR = ../objects

CFLAGS = -g -Wall

all : $(LIBDIR)/libclock.a

clock.o : clock.c clock.h
	$(CC) $(CFLAGS) -c clock.c

$(LIBDIR)/libclock.a : clock.o
	$(AR) cr $(LIBDIR)/libclock.a clock.o

clean :
	rm -f $(LIBDIR)/libclock.a clock.o
