
LIBDIR = ../../objects
BINDIR = ../..
DEVICEDIR = ../../device
FRAMEWORKDIR = ../../framework
DRIVERDIR = ..

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(FRAMEWORKDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

TARGETS= $(BINDIR)/test_kilo_0 $(BINDIR)/test_kilo_1 $(BINDIR)/test_kilo_2 \
	 $(BINDIR)/test_kilo_3 $(BINDIR)/test_kilo_4 $(BINDIR)/test_kilo_5

all : $(TARGETS)

$(BINDIR)/test_% : %.c $(DRIVERDIR)/driver.h \
		$(DEVICEDIR)/device_emu.h $(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< \
		 -lmain -ldevice -lframework -lkilo_tester

clean :
	rm -f $(TARGETS)
