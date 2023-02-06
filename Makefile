#
#
ELKS=../elks-gh
CFLAGS=-Wall -O3

all: sim-dos sim-elks test testdos

clean:
	-rm sim-dos sim-elks opcodes

test: sim-elks
	./sim-elks $(ELKS)/target/bin/banner ELKS Emulator
	@#./sim-elks $(ELKS)/target/bin/echo This is a test
	@#./sim-elks $(ELKS)/target/bin/printenv
	@#./sim-elks $(ELKS)/target/bin/env
	@#./sim-elks $(ELKS)/target/bin/hd 0:0#256
	@#./sim-elks $(ELKS)/target/bin/hd hd123
	@#./sim-elks $(ELKS)/target/bin/login
	@#./sim-elks $(ELKS)/target/bin/chmem $(ELKS)/target/bin/login

testdos: sim-dos
	./sim-dos test.exe
	@#./sim-dos hexdump.exe

sim-dos: sim.c 8086.c disasm.c loadexec-dos.c
	$(CC) $(CFLAGS) -DMSDOS=1 -o $@ $^

sim-elks: sim.c 8086.c disasm.c loadexec-elks.c
	$(CC) $(CFLAGS) -DELKS=1 -o $@ $^

opcodes: opcodes.S
	ia16-elf-gcc -melks -ffreestanding -nostdlib -o $@ $^
