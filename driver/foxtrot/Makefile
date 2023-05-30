
LIBDIR = ../../objects
BINDIR = ../..
DEVICEDIR = ../../device
FRAMEWORKDIR = ../../framework
DRIVERDIR = ..

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(FRAMEWORKDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

TARGETS = $(BINDIR)/test_foxtrot_0 $(BINDIR)/test_foxtrot_1 \
	$(BINDIR)/test_foxtrot_2

all : $(TARGETS)

$(BINDIR)/test_% : %.c $(DRIVERDIR)/driver.h \
		$(DEVICEDIR)/device_emu.h $(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< \
		 -lmain -ldevice -lframework -ltester

clean :
	rm -f $(TARGETS)
