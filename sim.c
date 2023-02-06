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
#include <stdarg.h>
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
    int b = readByte(ip, CS);
    if (!f_asmout) printf("%02x ", b);
    else f_outcol = 0;
    return b;
}

void runtimeError(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\nCS:IP = %04x:%04x\n", cs(), ip);
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
    load_executable(argv[1], argc, argv, environ);
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
