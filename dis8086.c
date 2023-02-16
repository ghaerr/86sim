/*
 * ELKS 8086 Disassembler
 *
 * Aug 2022 Greg Haerr
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#if __ia16__
#include <linuxmt/mem.h>
#include "instrument.h"
#endif
#include "exe.h"
#include "syms.h"
#include "disasm.h"
#include "discolor.h"

#define KSYMTAB     "/lib/system.sym"
#define MAGIC       0x0301  /* magic number for executable progs */

static int f_asmout;
static int f_octal;
static int f_ksyms;
static struct exe e;
static struct dis d;

#define peekb(cs,ip)  (*(unsigned char __far *)(((unsigned long)(cs) << 16) | (int)(ip)))

int nextbyte_mem(int cs, int ip)
{
    return peekb(cs, ip);
}

void disasm_mem(int cs, int ip, int opcount)
{
    int n;
    int nextip;

    int flags = f_asmout? fDisInst | fDisAsmSource : fDisCS | fDisIP | fDisBytes | fDisInst;
    if (f_octal) flags |= fDisOctal;
    if (!opcount) opcount = 32767;
    if (!f_asmout) printf("Disassembly of %s:\n", getsymbol(&d, cs, (int)ip));
    e.textseg = cs;
    for (n=0; n<opcount; n++) {
        nextip = disasm(&d, e.textseg, ip, nextbyte_mem, e.dataseg, flags);
        printf("%s\n", colorInst(&d, d.buf));
        if (opcount == 32767 && peekb(cs, ip) == 0xc3)  /* RET */
            break;
        ip = nextip;
    }
}

static FILE *infp;

static int nextbyte_file(int cs, int ip)
{
    int b = fgetc(infp);
#ifndef __ia16__
    if (b == EOF) exit(0);
#endif
    return b;
}

int disasm_file(char *filename)
{
    int ip = 0;
    int nextip;
    long filesize;
    struct stat sbuf;

    if (stat(filename, &sbuf) < 0 || (infp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "Can't open %s\n", filename);
        return 1;
    }
    filesize = sbuf.st_size;

    if (sym_read_exe_symbols(&e, filename) || (e.aout.type & 0xFFFF) == MAGIC) {
            fseek(infp, e.aout.hlen, SEEK_SET);
            filesize = e.aout.tseg;    /* FIXME no .fartext yet */
            e.textseg = 0;
            e.dataseg = 1;
    }

    int flags = f_asmout? fDisInst | fDisAsmSource : fDisAddr | fDisBytes | fDisInst;
    if (f_octal) flags |= fDisOctal;
    while (ip < filesize) {
        nextip = disasm(&d, e.textseg, ip, nextbyte_file, e.dataseg, flags);
        printf("%s\n", colorInst(&d, d.buf));
        ip = nextip;
    }
    fclose(infp);
    return 0;
}

void usage(void)
{
    fprintf(stderr, "Usage: disasm [-k] [-s symfile] [-A] [[seg:off[#size] | filename]\n");
    exit(1);
}

int main(int ac, char **av)
{
    unsigned long seg = 0, off = 0;
    int ch;
    char *symfile = NULL;
    long count = 22;

    while ((ch = getopt(ac, av, "kAos:")) != -1) {
        switch (ch) {
        case 'A':               /* output gnu as compatible input */
            f_asmout = 1;
            break;
        case 'o':
            f_octal = 1;
            break;
        case 'k':
            f_ksyms = 1;
            symfile = KSYMTAB;
            break;
        case 's':
            symfile = optarg;
            break;
        default:
            usage();
        }
    }
    ac -= optind;
    av += optind;
    if (ac < 1)
        usage();

    if (symfile && !sym_read_symbols(&e, symfile)) {
        fprintf(stderr, "Can't open %s\n", symfile);
        exit(1);
    }
#if __ia16__
    if (f_ksyms) {
        int fd = open("/dev/kmem", O_RDONLY);
        if (fd < 0
            || ioctl(fd, MEM_GETCS, &e.textseg) < 0
            || ioctl(fd, MEM_GETDS, &e.dataseg) < 0
            || ioctl(fd, MEM_GETFARTEXT, &e.ftextseg) < 0) {
                fprintf(stderr, "Can't get kernel segment values\n");
                exit(1);
        }
        close(fd);
    }
#endif

    d.e = &e;
    if (strchr(*av, ':')) {
        sscanf(*av, "%lx:%lx#%ld", &seg, &off, &count);

        if (seg > 0xffff || off > 0xffff) {
            fprintf(stderr, "Error: segment or offset larger than 0xffff\n");
            return 1;
        }
        disasm_mem((int)seg, (int)off, (int)count);
    } else return disasm_file(*av);
    return 0;
}
