
LIBDIR = ../objects
BINDIR = ..
DEVICEDIR = ../device
DRIVERDIR = ../driver

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

OBJECTS = framework.o fw_gpio.o fw_jumptable.o fw_execop.o fw_dib.o

all : $(LIBDIR)/libframework.a

fw_gpio.o : fw_gpio.c framework.h
	$(CC) $(CFLAGS) -c fw_gpio.c

fw_jumptable.o : fw_jumptable.c fw_jumptable.h framework.h \
		$(DEVICEDIR)/device_emu.h $(DRIVERDIR)/driver.h
	$(CC) $(CFLAGS) -c fw_jumptable.c

fw_execop.o : fw_execop.c fw_execop.h framework.h \
		$(DEVICEDIR)/device_emu.h $(DRIVERDIR)/driver.h
	$(CC) $(CFLAGS) -c fw_execop.c

fw_dib.o : fw_dib.c framework.h
	$(CC) $(CFLAGS) -c fw_dib.c

framework.o : framework.c framework.h fw_jumptable.h fw_execop.h \
		$(DEVICEDIR)/device_emu.h $(DRIVERDIR)/driver.h
	$(CC) $(CFLAGS) -c framework.c

$(LIBDIR)/libframework.a : $(OBJECTS)
	$(AR) cr $(LIBDIR)/libframework.a $(OBJECTS)


clean :
	rm -f $(LIBDIR)/libframework.a $(OBJECTS)
