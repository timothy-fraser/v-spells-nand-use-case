
LIBDIR = ../../objects
BINDIR = ../..
CLOCKDIR = ../../clock
DEVICEDIR = ../../device
FRAMEWORKDIR = ../../framework
SYSTESTDIR = ../../tester
DRIVERDIR = ..

CFLAGS = -g -Wall -I$(CLOCKDIR) -I$(DEVICEDIR) -I$(FRAMEWORKDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

TARGETS= $(BINDIR)/test_kilo_0 $(BINDIR)/test_kilo_1 $(BINDIR)/test_kilo_2 \
	 $(BINDIR)/test_kilo_3 $(BINDIR)/test_kilo_4 $(BINDIR)/test_kilo_5

all : $(TARGETS)

$(BINDIR)/test_% : %.c $(DRIVERDIR)/driver.h \
		$(CLOCKDIR)/clock.h $(LIBDIR)/libclock.a \
		$(DEVICEDIR)/device_emu.h $(LIBDIR)/libdevice.a \
		$(FRAMEWORKDIR)/framework.h $(LIBDIR)/libframework.a \
		$(SYSTESTDIR)/tester.h $(LIBDIR)/libsystemtest.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< \
		-lmain -lsystemtest -lframework -ldevice -lclock

clean :
	rm -f $(TARGETS)
