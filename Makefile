CC       = gcc
CFLAGS   = -Wall -g
LDFLAGS  =
BASE_OBJFILES = main.o device.o framework.o
DEVICE_TEST_OBJFILES = device_test.o device.o
ALPHA_TARGETS   = test_alpha_0 test_alpha_1 test_alpha_2 test_alpha_3 test_alpha_4 test_alpha_5 test_alpha_6
BRAVO_TARGETS   = test_bravo_0 test_bravo_1 test_bravo_2 test_bravo_3 test_bravo_4 test_bravo_5 test_bravo_6
FOXTROT_TARGETS = test_foxtrot_0 test_foxtrot_1 test_foxtrot_2
KILO_TARGETS    = test_kilo_0 test_kilo_1 test_kilo_2 test_kilo_3 test_kilo_4 test_kilo_5
DEVICE_TEST_TARGET = device_test

all: alpha bravo foxtrot kilo

alpha: $(ALPHA_TARGETS)

bravo: $(BRAVO_TARGETS)

foxtrot: $(FOXTROT_TARGETS)

kilo: $(KILO_TARGETS)

#
#
#                ALPHA
#
#

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_alpha_0: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/alpha/alpha_0.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_alpha_0 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_alpha_1: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/alpha/alpha_1.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_alpha_1 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_alpha_2: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/alpha/alpha_2.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_alpha_2 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_alpha_3: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/alpha/alpha_3.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_alpha_3 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_alpha_4: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/alpha/alpha_4.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_alpha_4 ./objects/*.o $(LDFLAGS)

	# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_alpha_5: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/alpha/alpha_5.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_alpha_5 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_alpha_6: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/alpha/alpha_6.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_alpha_6 ./objects/*.o $(LDFLAGS)

#
#
#                BRAVO
#
#

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_bravo_0: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/bravo/bravo_0.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_bravo_0 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_bravo_1: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/bravo/bravo_1.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_bravo_1 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_bravo_2: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/bravo/bravo_2.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_bravo_2 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_bravo_3: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/bravo/bravo_3.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_bravo_3 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_bravo_4: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/bravo/bravo_4.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_bravo_4 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_bravo_5: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/bravo/bravo_5.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_bravo_5 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_bravo_6: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/bravo/bravo_6.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_bravo_6 ./objects/*.o $(LDFLAGS)

#
#
#                FOXTROT
#
#

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_foxtrot_0: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/foxtrot/foxtrot_0.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_foxtrot_0 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_foxtrot_1: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/foxtrot/foxtrot_1.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_foxtrot_1 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_foxtrot_2: $(BASE_OBJFILES) tester.o
	$(CC) $(CFLAGS) -c ./driver/foxtrot/foxtrot_2.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_foxtrot_2 ./objects/*.o $(LDFLAGS)

#
#
#                KILO
#
#

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_kilo_0: $(BASE_OBJFILES) kilo_tester.o
	$(CC) $(CFLAGS) -c ./driver/kilo/kilo_0.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_kilo_0 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_kilo_1: $(BASE_OBJFILES) kilo_tester.o
	$(CC) $(CFLAGS) -c ./driver/kilo/kilo_1.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_kilo_1 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_kilo_2: $(BASE_OBJFILES) kilo_tester.o
	$(CC) $(CFLAGS) -c ./driver/kilo/kilo_2.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_kilo_2 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_kilo_3: $(BASE_OBJFILES) kilo_tester.o
	$(CC) $(CFLAGS) -c ./driver/kilo/kilo_3.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_kilo_3 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_kilo_4: $(BASE_OBJFILES) kilo_tester.o
	$(CC) $(CFLAGS) -c ./driver/kilo/kilo_4.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_kilo_4 ./objects/*.o $(LDFLAGS)

# Compile driver with framework/device references provided by headers
# Build driver with prerequisites and driver.o
test_kilo_5: $(BASE_OBJFILES) kilo_tester.o
	$(CC) $(CFLAGS) -c ./driver/kilo/kilo_5.c -I./driver -I./framework -I./device -o ./objects/driver.o
	$(CC) $(CFLAGS) -o test_kilo_5 ./objects/*.o $(LDFLAGS)


#
#
#                BASE
#
#

# Compile main with tester/device/framework references provided by headers
main.o:
	$(CC) $(CFLAGS) -c ./main/main.c -I./tester -I./device -I./framework -o ./objects/main.o

# Compile device
device.o:
	$(CC) $(CFLAGS) -c ./device/device_emu.c -I./device -I./framework -o ./objects/device.o

# Compile framework with driver/device references provided by headers
framework.o:
	$(CC) $(CFLAGS) -c ./framework/*.c -I./driver -I./framework -I./device -o ./objects/framework.o

# Compile tester with framework references provided by headers
tester.o:
	$(CC) $(CFLAGS) -c ./tester/tester.c -I./tester -I./framework -I./device -o ./objects/tester.o

# Compile kilo tester with framework references provided by headers
kilo_tester.o:
	$(CC) $(CFLAGS) -c ./tester/kilo_tester.c -I./tester -I./framework -I./driver -I./device -o ./objects/tester.o

# Compile kilo tester with framework references provided by headers
kilo_tester_6.o:
	$(CC) $(CFLAGS) -c ./tester/kilo_tester_6.c -I./tester -I./framework -I./driver -I./device -o ./objects/tester.o

#
#
#                DEVICE TEST
#
#

# Compile device test with device references provided by headers
device_test.o:
	$(CC) $(CFLAGS) -c ./device/device_test.c -I./device -I./framework -o ./device/device_test.o

# Build device with prerequisites and device.o
device_test: $(DEVICE_TEST_OBJFILES) 
	$(CC) $(CFLAGS) -o $(DEVICE_TEST_TARGET) ./device/device_test.o ./objects/device.o

#
#
#                CLEAN
#
#

clean:
	rm -f ./objects/* $(ALPHA_TARGETS) $(BRAVO_TARGETS) $(FOXTROT_TARGETS) $(KILO_TARGETS) $(DEVICE_TEST_TARGET) device/device_test.o
