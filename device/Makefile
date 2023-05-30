
LIBDIR = ../objects
BINDIR = ..
FRAMEWORKDIR = ../framework

CFLAGS = -g -Wall -I$(FRAMEWORKDIR)
LDFLAGS = -L $(LIBDIR)

all : $(LIBDIR)/libdevice.a $(BINDIR)/test_device

device_emu.o : device_emu.c device_emu.h $(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) -c device_emu.c

$(LIBDIR)/libdevice.a : device_emu.o
	$(AR) cr $(LIBDIR)/libdevice.a device_emu.o

$(BINDIR)/test_device : device_test.c $(LIBDIR)/libdevice.a \
		device_emu.h $(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BINDIR)/test_device device_test.c \
		-ldevice


clean :
	rm -f $(LIBDIR)/libdevice.a $(BINDIR)/test_device device_emu.o
