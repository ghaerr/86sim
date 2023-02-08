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

static bool f_disasm = 1;       /* do disassembly */
static bool f_showreps = 1;     /* show repeating instructions */
bool f_asmout = 0;              /* output gnu as compatible input */

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
    return readByte(ip, CS);
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
    struct dis d = {};

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
    int flags = f_asmout? fDisInst | fDisAsmSource : fDisCSIP | fDisBytes | fDisInst;
    Word lastIP = ip;
    for (;;) {
        if (f_disasm && (f_showreps || !repeating)) {
            disasm(&d, cs(), lastIP, nextbyte_mem, ds(), flags);
            printf("%s\n", d.buf);
        }
        ExecuteInstruction();
        if (!repeating)
            lastIP = ip;
    }
}
