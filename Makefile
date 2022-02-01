#
#
ELKS=../elks-gh
CFLAGS=-Wall -O3

all: 86sim-dos 86sim-elks test testdos

clean:
	-rm 86sim-dos 86sim-elks

test: 86sim-elks
	./86sim-elks $(ELKS)/target/bin/banner ELKS Emulator
	#./86sim-elks $(ELKS)/target/bin/echo This is a test
	#./86sim-elks $(ELKS)/target/bin/printenv
	#./86sim-elks $(ELKS)/target/bin/env
	#./86sim-elks $(ELKS)/target/bin/hd 0:0#256
	#./86sim-elks $(ELKS)/target/bin/hd hd123
	#./86sim-elks $(ELKS)/target/bin/login
	#./86sim-elks $(ELKS)/target/bin/chmem $(ELKS)/target/bin/login

testdos: 86sim-dos
	./86sim-dos test.exe
	#./86sim-dos hexdump.exe

86sim-dos: 86sim.c
	$(CC) $(CFLAGS) -DMSDOS=1 -o 86sim-dos 86sim.c 86disasm.c

86sim-elks: 86sim.c 86disasm.c
	$(CC) $(CFLAGS) -DELKS=1 -o 86sim-elks 86sim.c 86disasm.c