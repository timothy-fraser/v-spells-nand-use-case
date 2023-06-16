
LIBDIR = ../objects
BINDIR = ..
DEVICEDIR = ../device
FRAMEWORKDIR = ../framework
DRIVERDIR = ../driver

CFLAGS = -g -Wall -I$(DEVICEDIR) -I$(FRAMEWORKDIR) -I$(DRIVERDIR)
LDFLAGS = -L $(LIBDIR)

OBJS = st_data.o st_deterministic.o st_dib.o
STLIB = $(LIBDIR)/libsystemtest.a

# all : $(LIBDIR)/libtester.a $(LIBDIR)/libkilo_tester.a
all : $(STLIB)

st_data.o : st_data.c st_data.h
	$(CC) $(CFLAGS) -c st_data.c

st_deterministic.o : st_deterministic.c tester.h \
		$(FRAMEWORKDIR)/framework.h
	$(CC) $(CFLAGS) -c st_deterministic.c

st_dib.o : st_dib.c tester.h $(FRAMEWORKDIR)/framework.h \
		$(DEVICEDIR)/device_emu.h
	$(CC) $(CFLAGS) -c st_dib.c

$(STLIB) : $(OBJS)
	$(AR) cr $(STLIB) $(OBJS)

clean :
	rm -f $(STLIB) $(OBJS)
