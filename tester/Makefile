
LIBDIR = ../objects
BINDIR = ..
DEVICEDIR = ../device
FRAMEWORKDIR = ../framework
DRIVERDIR = ../driver

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(FRAMEWORKDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

all : $(LIBDIR)/libtester.a $(LIBDIR)/libkilo_tester.a

tester.o : tester.c tester.h $(DEVICEDIR)/device_emu.h \
		$(FRAMEWORKDIR)/framework.h $(DRIVERDIR)/driver.h
	$(CC) $(CFLAGS) -c tester.c

kilo_tester.o : kilo_tester.c tester.h $(DEVICEDIR)/device_emu.h \
		$(FRAMEWORKDIR)/framework.h $(DRIVERDIR)/driver.h
	$(CC) $(CFLAGS) -c kilo_tester.c

$(LIBDIR)/libtester.a : tester.o
	$(AR) cr $(LIBDIR)/libtester.a tester.o

$(LIBDIR)/libkilo_tester.a : kilo_tester.o
	$(AR) cr $(LIBDIR)/libkilo_tester.a kilo_tester.o


clean :
	rm -f $(LIBDIR)/libtester.a $(LIBDIR)/libkilo_tester.a \
		tester.o kilo_tester.o
