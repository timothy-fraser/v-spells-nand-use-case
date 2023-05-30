
all:
	cd device ; make
	cd framework ; make
	cd tester ; make
	cd main ; make
	cd driver ; make

clean:
	cd device ; make clean
	cd framework ; make clean
	cd tester ; make clean
	cd main ; make clean
	cd driver ; make clean
