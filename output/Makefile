
TIMEOUT=timeout

BINDIR = ..

TARGETS = \
	ioregs.txt device.txt mirror.txt \
	wait_alpha_0.txt wait_alpha_1.txt wait_alpha_2.txt wait_alpha_3.txt \
	wait_alpha_7.txt wait_alpha_8.txt \
	base_alpha_0.txt base_alpha_1.txt base_alpha_2.txt base_alpha_3.txt \
	base_alpha_4.txt base_alpha_5.txt base_alpha_6.txt base_alpha_7.txt \
	base_alpha_8.txt \
	fuzz_alpha_0.txt

all : $(TARGETS)

# Nothing much is going to work if these initial tests do not pass.
# Make the makefile halt if any of them fails.

ioregs.txt : $(BINDIR)/test_ioregs
	$(BINDIR)/test_ioregs > ioregs.txt

device.txt : $(BINDIR)/test_device
	$(BINDIR)/test_device > device.txt

mirror.txt : $(BINDIR)/test_mirror
	$(BINDIR)/test_mirror > mirror.txt

# Some of the following unit and system tests contain deliberate bugs;
# we expect them to fail and return a failure indication.  Use the
# magic "-" to tell the makefile to ignore the return values of these
# tests so it will execute them all despite the failures.

wait_%.txt : $(BINDIR)/test_wait_%
	- $< > $@

# Some of these tests will hang; use timeout to keep going.
base_%.txt : $(BINDIR)/test_%
	- $(TIMEOUT) --signal=TERM 10s $< --deterministic > $@

fuzz_%.txt : $(BINDIR)/test_%
	- $< --stochastic 2 > $@


clean :
	rm -f $(TARGETS)
