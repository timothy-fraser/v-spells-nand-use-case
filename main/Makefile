
LIBDIR = ../objects
BINDIR = ..
DEVICEDIR = ../device
FRAMEWORKDIR = ../framework
TESTERDIR = ../tester

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(FRAMEWORKDIR) -I$(TESTERDIR)
LDFLAGS = -L $(LIBDIR)

all : $(LIBDIR)/libmain.a

main.o : main.c $(TESTERDIR)/tester.h $(DEVICEDIR)/device_emu.h \
		$(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) -c main.c

$(LIBDIR)/libmain.a : main.o
	$(AR) cr $(LIBDIR)/libmain.a main.o


clean :
	rm -f $(LIBDIR)/libmain.a main.o
