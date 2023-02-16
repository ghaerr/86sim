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
#include "8086.h"
#include "exe.h"
#include "syms.h"
#include "disasm.h"
#include "discolor.h"

int f_verbose;
static int f_disasm;            /* do disassembly */
static int f_showreps = 1;      /* show repeating instructions */
static int f_octal;             /* display bytecodes in octal */
static char *program_file;

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

void usage(void)
{
    printf("Usage: %s [-vaoc] <program name>\n", program_file);
    exit(0);
}

int main(int argc, char *argv[])
{
    int ch;
    extern char **environ;
    struct exe e = {};
    struct dis d = {};

    program_file = argv[0];
    while ((ch = getopt(argc, argv, "vaoc")) != -1) {
        switch (ch) {
        case 'v':
            f_verbose = 1;
            break;
        case 'a':
            f_disasm = 1;
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

    initMachine(&e);
    load_executable(&e, argv[0], argc, argv, environ);
    sym_read_exe_symbols(&e, argv[0]);

#ifdef __APPLE__    /* macOS stdio drops characters! */
    static char buf[1];
    setvbuf(stdout, buf, _IOFBF, sizeof(buf));
#endif

    initExecute();
    int flags = fDisCS | fDisIP | fDisBytes | fDisInst;
    if (f_octal) flags |= fDisOctal;
    d.e = &e;
    e.textseg = cs();
    e.dataseg = ds();
    Word lastIP = getIP();
    for (;;) {
        if (f_disasm && (f_showreps || !isRepeating())) {
            disasm(&d, cs(), lastIP, nextbyte_mem, ds(), flags);
            printf("%s\n", colorInst(&d, d.buf));
        }
        executeInstruction();
        if (!isRepeating())
            lastIP = getIP();
    }
}
