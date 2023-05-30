
LIBDIR = ../objects
BINDIR = ..
DEVICEDIR = ../device
DRIVERDIR = ../driver

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

all : $(LIBDIR)/libframework.a

framework.o : framework.c framework.h \
		$(DEVICEDIR)/device_emu.h $(DRIVERDIR)/driver.h
	$(CC) $(CFLAGS) -c framework.c

$(LIBDIR)/libframework.a : framework.o
	$(AR) cr $(LIBDIR)/libframework.a framework.o


clean :
	rm -f $(LIBDIR)/libframework.a framework.o
