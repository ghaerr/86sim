/*
 * 8086 emulator
 *
 * Orginally from Andrew Jenner's reenigne project
 * DOS enhancements by TK Chia
 * ELKS executable support by Greg Haerr
 * Disassembler added by Greg Haerr
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "sim.h"
#include "disasm.h"

bool f_disasm = 1;      /* do disassembly */
bool f_asmout = 0;      /* output gnu as compatible input */
bool f_showreps = 1;    /* show repeating instructions */

char *getsymbol(int seg, int offset)
{
    static char buf[8];

    sprintf(buf, f_asmout? "0x%04x": "%04x", offset);
    return buf;
}

char *getsegsymbol(int seg)
{
    static char buf[8];

    sprintf(buf, f_asmout? "0x%04x": "%04x", seg);
    return buf;
}

int nextbyte_mem(int cs, int ip)
{
    int b = readByte(ip, CS);     /* seg =1 for CS */
    if (!f_asmout) printf("%02x ", b);
    else f_outcol = 0;
    return b;
}

void error(const char* operation)
{
    fprintf(stderr, "Error %s file %s: %s\n", operation, filename, strerror(errno));
    exit(1);
}

void runtimeError(const char* message)
{
    fprintf(stderr, "%s\nCS:IP = %04x:%04x\n", message, cs(), ip);
    exit(1);
}

void divideOverflow()
{
    runtimeError("Divide overflow");
}

int main(int argc, char* argv[])
{
    extern char **environ;

    if (argc < 2) {
        printf("Usage: %s <program name>\n", argv[0]);
        exit(0);
    }
    initMachine();

    filename = argv[1];
    FILE* fp = fopen(filename, "rb");
    if (fp == 0)
        error("opening");
    if (fseek(fp, 0, SEEK_END) != 0)
        error("seeking");
    filesize = ftell(fp);
    if (filesize == -1)
        error("telling");
    if (fseek(fp, 0, SEEK_SET) != 0)
        error("seeking");
#if ELKS
    loadSegment = 0x1000 - 2;
#endif
#if MSDOS
    loadSegment = 0x0212;
#endif
    int loadOffset = loadSegment << 4;
    if (filesize > 0x100000 - loadOffset)
        filesize = 0x100000 - loadOffset;
    if (fread(&ram[loadOffset], filesize, 1, fp) != 1)
        error("reading");
    fclose(fp);

    load_executable(fp, filesize, argc, argv, environ);
    load_bios_irqs();
    set_entry_registers();

#ifdef __APPLE__    /* macOS stdio drops characters! */
    static char buf[1];
    setvbuf(stdout, buf, _IOFBF, sizeof(buf));
#endif

    initExecute();
    Word lastIP = ip;
    for (;;) {
        if (f_disasm && (f_showreps || !repeating)) {
            if (!f_asmout) printf("%04hx:%04hx  ", cs(), lastIP);
            disasm(cs(), lastIP, nextbyte_mem, ds());
        }
        ExecuteInstruction();
        if (!repeating)
            lastIP = ip;
    }
}
