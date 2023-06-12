
LIBDIR = ../objects
BINDIR = ..
DEVICEDIR = ../device
DRIVERDIR = ../driver

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

OBJECTS = framework.o gpio.o

all : $(LIBDIR)/libframework.a

gpio.o : gpio.c gpio.h
	$(CC) $(CFLAGS) -c gpio.c

framework.o : framework.c framework.h gpio.h \
		$(DEVICEDIR)/device_emu.h $(DRIVERDIR)/driver.h
	$(CC) $(CFLAGS) -c framework.c

$(LIBDIR)/libframework.a : $(OBJECTS)
	$(AR) cr $(LIBDIR)/libframework.a $(OBJECTS)


clean :
	rm -f $(LIBDIR)/libframework.a framework.o
