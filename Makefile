#
#
ELKS=../elks-gh
CFLAGS=-Wall -O3

all: sim-dos sim-elks dis8086 nm86 test testdos

clean:
	-rm -f sim-dos sim-elks dis8086 opcodes

test2: sim-elks
	./sim-elks -a echo1

test: sim-elks
	./sim-elks -v $(ELKS)/target/bin/banner ELKS Emulator
	@#./sim-elks $(ELKS)/target/bin/echo This is a test
	@#./sim-elks $(ELKS)/target/bin/printenv
	@#./sim-elks $(ELKS)/target/bin/env
	@#./sim-elks $(ELKS)/target/bin/hd 0:0#256
	@#./sim-elks $(ELKS)/target/bin/hd hd123
	@#./sim-elks $(ELKS)/target/bin/login
	@#./sim-elks $(ELKS)/target/bin/chmem $(ELKS)/target/bin/login

testdos: sim-dos
	./sim-dos -va test.exe
	@#./sim-dos hexdump.exe

sim-dos: sim.c 8086.c disasm.c dissim.c loadexec-dos.c syms.c colorinst.c
	$(CC) $(CFLAGS) -DMSDOS=1 -o $@ $^

sim-elks: sim.c 8086.c disasm.c dissim.c loadexec-elks.c syms.c colorinst.c
	$(CC) $(CFLAGS) -DELKS=1 -o $@ $^

dis8086: dis8086.c disasm.c dissim.c syms.c colorinst.c
	$(CC) $(CFLAGS) -D__far= -Wno-format -o $@ $^

nm86: nm86.c syms.c
	$(CC) $(CFLAGS) -o $@ $^

opcodes: opcodes.S
	ia16-elf-gcc -melks -ffreestanding -nostdlib -o $@ $^
