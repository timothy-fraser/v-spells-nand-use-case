
LIBDIR = ../../objects
BINDIR = ../..
CLOCKDIR = ../../clock
DEVICEDIR = ../../device
FRAMEWORKDIR = ../../framework
DRIVERDIR = ..
ALPHADIR = $(DRIVERDIR)/alpha

CFLAGS = -g -Wall -I$(CLOCKDIR) -I$(DEVICEDIR) -I$(FRAMEWORKDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

TARGETS = $(BINDIR)/test_wait_alpha_0 $(BINDIR)/test_wait_alpha_1 \
		$(BINDIR)/test_wait_alpha_2 $(BINDIR)/test_wait_alpha_3 \
		$(BINDIR)/test_wait_alpha_7 $(BINDIR)/test_wait_alpha_8

all : $(TARGETS)

test_wait.o : test_wait.c $(FRAMEWORKDIR)/framework.h $(DRIVERDIR)/driver.h \
		$(DEVICEDIR)/device_emu.h $(CLOCKDIR)/clock.h
	$(CC) $(CFLAGS) -c test_wait.c

$(BINDIR)/test_wait_% : $(ALPHADIR)/%.c test_wait.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test_wait.o $< -lclock

clean :
	rm -f test_wait.o $(TARGETS)
