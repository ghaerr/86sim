#
#
ELKS=../elks-gh
CXXFLAGS=-Wall -O3

all: 86sim-dos 86sim-elks test

clean:
	-rm 86sim-dos 86sim-elks

test: 86sim-elks
	./86sim-elks $(ELKS)/target/bin/banner ELKS Emulator
	#./86sim-elks $(ELKS)/target/bin/hd 0:0#256
	#./86sim-elks $(ELKS)/target/bin/hd hd123
	./86sim-elks $(ELKS)/target/bin/echo This is a test
	./86sim-elks $(ELKS)/target/bin/printenv
	#./86sim-elks $(ELKS)/target/bin/env
	#./86sim-elks $(ELKS)/target/bin/login
	./86sim-elks $(ELKS)/target/bin/chmem $(ELKS)/target/bin/login

test2: 86sim-dos
	./86sim-dos test.exe

diff:
	diff -u 86sim.cpp 86sim-elks.cpp

diff2:
	diff -u 86sim-tkchia.cpp 86sim-dos.cpp

diff3:
	diff -u 86sim.cpp 86sim-tkchia.cpp

testdos: 86sim-dos
	./86sim-dos test.exe
	#./86sim-dos hexdump.exe

86sim-dos: 86sim.cpp
	$(CXX) $(CXXFLAGS) -DMSDOS=1 -o 86sim-dos 86sim.cpp

86sim-elks: 86sim.cpp
	$(CXX) $(CXXFLAGS) -DELKS=1 -o 86sim-elks 86sim.cpp
