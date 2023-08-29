
This directory's Makefile will run all of the unit and system tests
and save their outputs in text files.  The text files that ship with
the distribution represent the expected correct output of these tests.

Contents:

README.txt       - this file.
Makefile         - the makefile that runs all the unit and system tests.

ioregs.txt       - output of test_ioregs unit test.
device.txt       - output of test_device unit test, empty if all passed.
mirror.txt       - output of test_mirror unit test.
wait_alpha_?.txt - output of test_wait_alpha_? unit tests.

base_?.txt       - output of all driver system tests in deterministic mode.
fuzz_alpha_0.txt - output of alpha_0 driver system test in stochastic mode.
