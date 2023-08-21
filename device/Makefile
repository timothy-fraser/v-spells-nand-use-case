
LIBDIR = ../objects
BINDIR = ..
CLOCKDIR = ../clock
FRAMEWORKDIR = ../framework

CFLAGS = -g -Wall -I$(CLOCKDIR) -I$(FRAMEWORKDIR)
LDFLAGS = -L $(LIBDIR)

OBJS =  de_deadline.o de_store.o de_parser.o de_gpio.o de_ioregs.o de_device.o 

all : $(LIBDIR)/libdevice.a $(BINDIR)/test_ioregs $(BINDIR)/test_device

de_deadline.o : de_deadline.c de_deadline.h $(CLOCKDIR)/clock.h
	$(CC) $(CFLAGS) -c de_deadline.c
#	$(CC) $(CFLAGS) -DDIAGNOSTICS_SET -c de_deadline.c
#	$(CC) $(CFLAGS) -DDIAGNOSTICS_GET -c de_deadline.c

de_store.o : de_store.c de_store.h
	$(CC) $(CFLAGS) -c de_store.c

de_parser.o : de_parser.c de_parser.h de_store.h de_deadline.h \
		$(CLOCKDIR)/clock.h
	$(CC) $(CFLAGS) -c de_parser.c
#	$(CC) $(CFLAGS) -DDIAGNOSTICS -c de_parser.c

de_gpio.o : de_gpio.c de_gpio.h
	$(CC) $(CFLAGS) -c de_gpio.c

de_ioregs.o : de_ioregs.c de_ioregs.h
	$(CC) $(CFLAGS) -c de_ioregs.c

de_device.o : de_device.c device_emu.h \
		de_gpio.h \
		$(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) -c de_device.c

$(LIBDIR)/libdevice.a : $(OBJS)
	$(AR) cr $(LIBDIR)/libdevice.a $(OBJS)

$(BINDIR)/test_device : test_device.c $(LIBDIR)/libdevice.a \
		device_emu.h $(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BINDIR)/test_device test_device.c \
		-ldevice -lclock

$(BINDIR)/test_ioregs : test_ioregs.c test_patterns.h test_patterns.s \
		de_ioregs.o
	$(CC) $(CFLAGS) -o $(BINDIR)/test_ioregs test_ioregs.c \
		test_patterns.s de_ioregs.o

clean :
	rm -f $(LIBDIR)/libdevice.a $(BINDIR)/test_device \
		$(BINDIR)/test_ioregs $(OBJS)
