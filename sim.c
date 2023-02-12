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
#include <getopt.h>
#include "sim.h"
#include "syms.h"
#include "disasm.h"
#include "colorinst.h"

static int f_disasm;            /* do disassembly */
static int f_showreps = 1;      /* show repeating instructions */
int f_asmout;                   /* output gnu as compatible input */
int f_octal;                    /* display bytecodes in octal */
int f_syms;
static unsigned short textseg, dataseg;
char *program_file;

char * getsymbol(int seg, int offset)
{
    char *p;
    static char buf[64];

    if (f_syms) {
        if (seg == dataseg) {
            p = HighStart(buf, g_high.symbol);
            p = stpcpy(p, sym_data_symbol(offset, 1));
            p = HighEnd(p);
            *p = '\0';
            return buf;
        }
#if 0
        if (seg == ftextseg) {
            p = HighStart(buf, g_high.symbol);
            p = stpcpy(p, sym_ftext_symbol(offset, 1));
            p = HighEnd(p);
            *p = '\0';
            return buf;
        }
#endif
        //if (seg == textseg) {
            p = HighStart(buf, g_high.symbol);
            p = stpcpy(p, sym_text_symbol(offset, 1));
            p = HighEnd(p);
            *p = '\0';
            return buf;
        //}
    }

    sprintf(buf, f_asmout? "0x%04x": "%04x", offset);
    return buf;
}

char * getsegsymbol(int seg)
{
    static char buf[8];

    if (f_syms) {
        if (seg == textseg)
            return ".text";
        //if (seg == ftextseg)
            //return ".fartext";
        if (seg == dataseg)
            return ".data";
    }

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
    fprintf(stderr, "\nCS:IP = %04x:%04x\n", cs(), getIP());
    exit(1);
}

void divideOverflow()
{
    runtimeError("Divide overflow");
}

void usage(void)
{
    printf("Usage: %s [-aAo] <program name>\n", program_file);
    exit(0);
}

int main(int argc, char *argv[])
{
    int ch;
    extern char **environ;
    struct dis d = {};

    program_file = argv[0];
    while ((ch = getopt(argc, argv, "aAoc")) != -1) {
        switch (ch) {
        case 'a':
            f_disasm = 1;
            break;
        case 'A':
            f_asmout = 1;
            break;
        case 'o':
            f_octal = 1;
            break;
        case 'c':
            g_high.enabled = 0;
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 1)
        usage();

    initMachine();
    f_syms = sym_read_exe_symbols(argv[0])? 1: 0;
    load_executable(argv[0], argc, argv, environ);

#ifdef __APPLE__    /* macOS stdio drops characters! */
    static char buf[1];
    setvbuf(stdout, buf, _IOFBF, sizeof(buf));
#endif

    initExecute();
    int flags = f_asmout? fDisInst | fDisAsmSource : fDisCS | fDisIP | fDisBytes | fDisInst;
    if (f_octal) flags |= fDisOctal;

    textseg = cs();
    dataseg = ds();
    Word lastIP = getIP();
    for (;;) {
        if (f_disasm && (f_showreps || !isRepeating())) {
            disasm(&d, cs(), lastIP, nextbyte_mem, ds(), flags);
            printf("%s\n", ColorInst(&d, d.buf));
        }
        ExecuteInstruction();
        if (!isRepeating())
            lastIP = getIP();
    }
}
