# sim86 8086 emulator Makefile
#
ELKS=../elks-gh
CFLAGS=-Wall -O3

all: sim86 dis86 nm86 test testdos

clean:
	-rm -f sim86 dis86 opcodes

test2: sim86
	sim86 -a echo1

test: sim86
	./sim86 -v $(ELKS)/target/bin/banner ELKS Emulator
	@#./sim86 $(ELKS)/target/bin/hd sim86
	@#./sim86 $(ELKS)/target/bin/cat sim86.c
	@#./sim86 $(ELKS)/target/bin/echo This is a test
	@#./sim86 $(ELKS)/target/bin/printenv
	@#./sim86 $(ELKS)/target/bin/env
	@#./sim86 $(ELKS)/target/bin/hd 0:0#256
	@#./sim86 $(ELKS)/target/bin/login
	@#./sim86 $(ELKS)/target/bin/chmem $(ELKS)/target/bin/login

testdos: sim86
	./sim86 -va test.exe
	@#./sim-dos hexdump.exe

testbin: sim86
	./sim86 -va basic.bin

sim86: sim86.c 8086.c disasm.c dissim.c discolor.c syms.c loader-elks.c syscall-elks.c loader-dos.c syscall-dos.c loader-bin.c
	$(CC) $(CFLAGS) -o $@ $^

dis86: dis86.c disasm.c dissim.c discolor.c syms.c
	$(CC) $(CFLAGS) -D__far= -Wno-format -o $@ $^

nm86: nm86.c syms.c
	$(CC) $(CFLAGS) -o $@ $^

opcodes: opcodes.S
	ia16-elf-gcc -melks -ffreestanding -nostdlib -o $@ $^
