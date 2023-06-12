
LIBDIR = ../../objects
BINDIR = ../..
DEVICEDIR = ../../device
FRAMEWORKDIR = ../../framework
DRIVERDIR = ..

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(FRAMEWORKDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

TARGETS= $(BINDIR)/test_alpha_0 $(BINDIR)/test_alpha_1 $(BINDIR)/test_alpha_2 \
	 $(BINDIR)/test_alpha_3 $(BINDIR)/test_alpha_4 $(BINDIR)/test_alpha_5 \
	 $(BINDIR)/test_alpha_6 $(BINDIR)/test_alpha_7 $(BINDIR)/test_alpha_8

all : $(TARGETS)

$(BINDIR)/test_% : %.c $(DRIVERDIR)/driver.h \
		$(DEVICEDIR)/device_emu.h $(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< \
		-lmain -ltester -lframework -ldevice

clean :
	rm -f $(TARGETS)
