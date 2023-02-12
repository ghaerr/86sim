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
#include "syms.h"
#include "disasm.h"

#define KSYMTAB     "/lib/system.sym"
#define MAGIC       0x0301  /* magic number for executable progs */

static char f_asmout = 0;   /* output gnu as compatible input */
static char f_octal;
static char f_ksyms;
static char f_syms;
static unsigned short textseg, ftextseg, dataseg;
static struct dis d;

char * noinstrument getsymbol(int seg, int offset)
{
    static char buf[8];

    if (f_ksyms) {
        if (seg == textseg)
            return sym_text_symbol(offset, 1);
        if (seg == ftextseg)
            return sym_ftext_symbol(offset, 1);
        if (seg == dataseg)
            return sym_data_symbol(offset, 1);
    }
    if (f_syms) {
        if (seg == dataseg)
            return sym_data_symbol(offset, 1);
        return sym_text_symbol(offset, 1);
    }

    sprintf(buf, f_asmout? "0x%04x": "%04x", offset);
    return buf;
}

char * noinstrument getsegsymbol(int seg)
{
    static char buf[8];

    if (f_ksyms) {
        if (seg == textseg)
            return ".text";
        if (seg == ftextseg)
            return ".fartext";
        if (seg == dataseg)
            return ".data";
    }
    if (f_syms) {
        if (seg == dataseg)
            return ".data";
        return ".text";
    }

    sprintf(buf, f_asmout? "0x%04x": "%04x", seg);
    return buf;
}

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
    if (!f_asmout) printf("Disassembly of %s:\n", getsymbol(cs, (int)ip));
    for (n=0; n<opcount; n++) {
        nextip = disasm(&d, cs, ip, nextbyte_mem, dataseg, flags);
        printf("%s\n", d.buf);
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

    f_syms = sym_read_exe_symbols(filename)? 1: 0;
    if (f_syms || (sym_hdr.type & 0xFFFF) == MAGIC) {
            fseek(infp, sym_hdr.hlen, SEEK_SET);
            filesize = sym_hdr.tseg;    /* FIXME no .fartext yet */
            textseg = 0;
            dataseg = 1;
    }

    int flags = f_asmout? fDisInst | fDisAsmSource : fDisAddr | fDisBytes | fDisInst;
    if (f_octal) flags |= fDisOctal;
    while (ip < filesize) {
        nextip = disasm(&d, textseg, ip, nextbyte_file, dataseg, flags);
        printf("%s\n", d.buf);
        ip = nextip;
    }
    fclose(infp);
    return 0;
}

void usage(void)
{
    fprintf(stderr, "Usage: disasm [-k] [-a] [-s symfile] [[seg:off[#size] | filename]\n");
    exit(1);
}

int main(int ac, char **av)
{
    unsigned long seg = 0, off = 0;
    int ch;
    char *symfile = NULL;
    long count = 22;

    while ((ch = getopt(ac, av, "kaos:")) != -1) {
        switch (ch) {
        case 'a':
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
            f_syms = 1;
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

#if __ia16__
    if (symfile && !sym_read_symbols(symfile)) {
        fprintf(stderr, "Can't open %s\n", symfile);
        exit(1);
    }

    if (f_ksyms) {
        int fd = open("/dev/kmem", O_RDONLY);
        if (fd < 0
            || ioctl(fd, MEM_GETCS, &textseg) < 0
            || ioctl(fd, MEM_GETDS, &dataseg) < 0
            || ioctl(fd, MEM_GETFARTEXT, &ftextseg) < 0) {
                fprintf(stderr, "Can't get kernel segment values\n");
                exit(1);
        }
        close(fd);
    }
#endif

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
