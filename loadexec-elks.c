/* loadexec-elks.c - ELKS-specific functions for 8086 simulator */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "sim.h"

/* loader globals */
static const char* filename;
Word loadSegment;
DWord stackLow;

static unsigned int sysbrk;

#if 0
/* hex dump library*/
#define isprint(c) ((c) > ' ' && (c) <= '~')
static int lastnum[16] = {-1};
static int lastaddr = -1;

static void printline(int address, int *num, char *chr, int count, int summary)
{
    int   j;

    if (lastaddr >= 0)
    {
        for (j = 0; j < count; j++)
            if (num[j] != lastnum[j])
                break;
        if (j == 16 && summary)
        {
            if (lastaddr + 16 == address)
            {
                printf("*\n");
            }
            return;
        }
    }

    lastaddr = address;
    printf("%04x:", address);
    for (j = 0; j < count; j++)
    {
        if (j == 8)
            putchar(' ');
        if (num[j] >= 0)
            printf(" %02x", num[j]);
        else
            printf("   ");
        lastnum[j] = num[j];
        num[j] = -1;
    }

    for (j=count; j < 16; j++)
    {
        if (j == 8)
            putchar(' ');
        printf("   ");
    }

    printf("  ");
    for (j = 0; j < count; j++)
        printf("%c", chr[j]);
    printf("\n");
}

void hexdump(int startoff, unsigned char *addr, int count, int summary)
{
    int offset;
    char buf[20];
    int num[16];

    for (offset = startoff; count > 0; count -= 16, offset += 16)
    {
        int j, ch;

        memset(buf, 0, 16);
        for (j = 0; j < 16; j++)
            num[j] = -1;
        for (j = 0; j < 16; j++)
        {
            ch = *addr++;

            num[j] = ch;
            if (isprint(ch) && ch < 0x80)
                buf[j] = ch;
            else
                buf[j] = '.';
        }
        printline(offset, num, buf, count > 16? 16: count, summary);
    }
}
#endif

static void write_environ(int argc, char **argv, char **envp)
{
    char **p;
    int argv_len=0, argv_count=0;
    int envp_len=0, envp_count=0;
    int stack_bytes;
    int pip;
    int pcp, baseoff;

    /* How much space for argv */
    for(p=argv; p && *p && argv_len >= 0; p++)
    {
        argv_count++;
        argv_len += strlen(*p)+1;
    }

    /* How much space for envp */
    for(p=envp; p && *p && envp_len >= 0; p++)
    {
        envp_count++;
        envp_len += strlen(*p)+1;
    }

    /* tot it all up */
    stack_bytes = 2             /* argc */
        + argv_count * 2 + 2    /* argv */
        + argv_len
        + envp_count * 2 + 2    /* envp */
        + envp_len;

    if (!f_asmout)
        printf("argv = (%d,%d), envp=(%d,%d), size=0x%x\n",
            argv_count, argv_len, envp_count, envp_len, stack_bytes);

    stack_bytes = (stack_bytes + 1) & ~1;

    setSP(sp() - stack_bytes);
    int stk_ptr = sp();

    /* Now copy in the strings */
    pip = stk_ptr;
    pcp = stk_ptr+2*(1+argv_count+1+envp_count+1);

    /* baseoff = stk_ptr + stack_bytes; */
    /* baseoff = stk_ptr; */
    baseoff = 0;
    writeWord(argv_count, pip, SS); pip += 2;
    for(p=argv; p && *p; p++)
    {
       int n;

       writeWord(pcp-baseoff, pip, SS); pip += 2;
       n = strlen(*p)+1;
       for (int i = 0; i<n; i++)
        writeByte((*p)[i], pcp+i, SS);
       pcp += n;
    }
    writeWord(0, pip, SS);  pip += 2;

    for(p=envp; p && *p; p++)
    {
       int n;

       writeWord(pcp-baseoff, pip, SS); pip += 2;
       n = strlen(*p)+1;
       for (int i = 0; i<n; i++)
        writeByte((*p)[i], pcp+i, SS);
       pcp += n;
    }
    writeWord(0, pip, SS);  pip += 2;
}

static void error(const char* operation)
{
    fprintf(stderr, "Error %s file %s: %s\n", operation, filename, strerror(errno));
    exit(1);
}

void load_executable(const char *path, int argc, char **argv, char **envp)
{
    filename = path;
    FILE* fp = fopen(filename, "rb");
    if (fp == 0)
        error("opening");
    if (fseek(fp, 0, SEEK_END) != 0)
        error("seeking");
    int filesize = ftell(fp);
    if (filesize == -1)
        error("telling");
    if (fseek(fp, 0, SEEK_SET) != 0)
        error("seeking");

    loadSegment = 0x1000 - 2;
    int loadOffset = loadSegment << 4;
    if (filesize > 0x100000 - loadOffset)
        filesize = 0x100000 - loadOffset;
    if (fread(&ram[loadOffset], filesize, 1, fp) != 1)
        error("reading");
    fclose(fp);

    setFlags(0x3202);
    // FIXME check hlen < 0x20, unset hdr access after, check tseg & 15
    for (int i = 0; i < 0x20; ++i) {
        setES(loadSegment + (i >> 4));
        physicalAddress(i & 15, ES, true);
    }
    setES(loadSegment);
    setCS(loadSegment);
    setSS(loadSegment);
    setDS(loadSegment);
    int hlen = readWord(0x04);
    int version = readWord(0x06);
    int tseg = readWord(0x08);
    int dseg = readWord(0x0C);
    int bseg = readWord(0x10);
    int entry = readWord(0x14);
    int chmem = readWord(0x18);
    int minstack = readWord(0x1C);
    if (!f_asmout)
        printf("hlen %x version %x tseg %x dseg %x bseg %x entry %x chmem %x minstack %x\n",
        hlen, version, tseg, dseg, bseg, entry, chmem, minstack);
    for (int i = hlen; i < filesize+bseg+8192; ++i) {
        setES(loadSegment + (i >> 4));
        physicalAddress(i & 15, ES, true);
    }
    setCS(loadSegment + (hlen>>4));
    setSS(loadSegment + (hlen>>4) + ((tseg + 15) >> 4));
    setDS(ss());                /* DS = SS */
    sysbrk = dseg + bseg + 4096;
    int stack = sysbrk + 4096;
    //int stack = 0xfffe;
    setSP(stack);
    write_environ(argc, argv+1, envp);
    //hexdump(sp(), &ram[physicalAddress(sp(), SS, false)], stack-sp(), 0);
    //int extra = stack - sp();
    if (!f_asmout)
        printf("Text %x Data %x Stack %x\n", tseg, dseg+bseg+4096, 4096);
    ip = entry;

    for (int i=dseg; i<dseg+bseg; i++)  /* clear BSS */
        writeByte(0, i, DS);
}

void set_entry_registers(void)
{
    if (!f_asmout)
        printf("CS:IP %x:%x DS %x SS:SP %x:%x\n", cs(), ip, ds(), ss(), sp());
    setES(ds());        /* ES = DS */
    setAX(0x0000);
    setBX(0x0000);
    setCX(0x0000);
    setDX(0x0000);
    setBP(0x0000);
    setSI(0x0000);
    setDI(0x0000);
}

void load_bios_irqs(void)
{
}

void handle_intcall(int intno)
{
    unsigned int v;
    unsigned char *p;

    if (intno != 0x80)
        runtimeError("Unknown INT 0x%02x", intno);
    //fflush(stdout);
    /* syscall args: BX, CX, DX, DI, SI */
    switch (ax()) {
    case 1:             // exit
        printf("EXIT %d\n", bx());
        exit(bx());
    case 4:             // write
        p = &ram[physicalAddress(cx(), SS, false)];
        if (f_asmout) v = dx();
        else v = write(bx(), p, dx());
        setAX(v);
        break;
    case 5:             // open
        p = &ram[physicalAddress(bx(), SS, false)];
        printf("open '%s',%x,%o\n", p, cx(), dx());
        setAX(-2);
        break;
    case 54:            // ioctl
        if (!f_asmout)
            printf("IOCTL %d,%c%02d,%x\n", bx(), cx()>>8, cx()&0xff, dx());
        setAX(bx() < 3? 0: -1);
        break;
    case 17:            // brk
        printf("BRK old %x new %x\n", sysbrk, bx());
        sysbrk = bx();
        setAX(0);
        break;
    case 69:            // sbrk
        printf("SBRK %d old %x new %x SP %x\n", bx(), sysbrk, sysbrk+bx(), sp());
        v = sysbrk;
        sysbrk += bx();
        writeWord(v, cx(), SS);
        setAX(0);
        break;
    default:
        fprintf(stderr, "Unknown SYS call: AX %04x(%d) BX %04x CX %04x DX %04x\n",
            ax(), ax(), bx(), cx(), dx());
        //runtimeError("");
        setAX(0xffff);
        break;
    }
}
