
LIBDIR = ../../objects
BINDIR = ../..
CLOCKDIR = ../../clock
DEVICEDIR = ../../device
FRAMEWORKDIR = ../../framework
SYSTESTDIR = ../../tester
DRIVERDIR = ..

CFLAGS = -g -Wall -I$(CLOCKDIR) -I$(DEVICEDIR) -I$(FRAMEWORKDIR) \
		-I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

TARGETS= $(BINDIR)/test_alpha_0 $(BINDIR)/test_alpha_1 $(BINDIR)/test_alpha_2 \
	 $(BINDIR)/test_alpha_3 $(BINDIR)/test_alpha_4 $(BINDIR)/test_alpha_5 \
	 $(BINDIR)/test_alpha_6 $(BINDIR)/test_alpha_7 $(BINDIR)/test_alpha_8

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
