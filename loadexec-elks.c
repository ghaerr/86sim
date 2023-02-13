/* loadexec-elks.c - ELKS-specific functions for 8086 simulator */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "sim.h"
#include "exe.h"

/* loader globals */
Word loadSegment;
DWord stackLow;
extern int f_verbose;

static const char* filename;
static unsigned int sysbrk;

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

    if (f_verbose)
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

static void loadError(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    exit(1);
}

static void set_entry_registers(void)
{
    if (f_verbose) printf("CS:IP %x:%x DS %x SS:SP %x:%x\n",
        cs(), getIP(), ds(), ss(), sp());
    setES(ds());        /* ES = DS */
    setAX(0x0000);
    setBX(0x0000);
    setCX(0x0000);
    setDX(0x0000);
    setBP(0x0000);
    setSI(0x0000);
    setDI(0x0000);
    setFlags(0x3202);
}

static void load_bios_irqs(void)
{
}

void load_executable(struct exe *e, const char *path, int argc, char **argv, char **envp)
{
    filename = path;
    struct stat sbuf;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        loadError("Can't open %s\n", path);
    if (read(fd, &e->aout, sizeof(e->aout)) != sizeof(e->aout))
        loadError("Can't read header: %s\n", path);
    if ((e->aout.type & 0xFFFF) != ELKSMAGIC)
        loadError("%s: not ELKS executable\n");
    if (e->aout.hlen != sizeof(e->aout))
        loadError("Medium model programs not yet supported: %s\n", path);
    if (fstat(fd, &sbuf) < 0)
        loadError("Can't stat %s\n", path);
    size_t filesize = sbuf.st_size - e->aout.syms - e->aout.hlen;

    loadSegment = 0x1000;
    int loadOffset = loadSegment << 4;
    if (filesize > RAMSIZE - loadOffset)
        loadError("Not enough memory to load %s, needs %d bytes have %d\n",
            path, filesize, RAMSIZE);
    if (read(fd, &ram[loadOffset], filesize) != filesize)
        loadError("Error reading executable: %s\n", path);
    close(fd);

    unsigned int tseg = e->aout.tseg;
    unsigned int dseg = e->aout.dseg;
    unsigned int bseg = e->aout.bseg;
    unsigned int entry = e->aout.entry;
    unsigned int chmem = e->aout.chmem;
    unsigned int minstack = e->aout.minstack;
    if (f_verbose)
        printf("tseg %04x dseg %04x bseg %04x entry %x chmem %x minstack %x\n",
            tseg, dseg, bseg, entry, chmem, minstack);
    setES(loadSegment);
    //FIXME don't allow stack reads before written
    setShadowFlags(0, ES, filesize+bseg+8192, fRead|fWrite);
    setCS(loadSegment);
    setIP(entry);
    setSS(loadSegment + ((tseg + 15) >> 4));
    setDS(ss());                /* DS = SS */
    sysbrk = dseg + bseg + 4096;
    int stack = sysbrk + 4096;
    //int stack = 0xfffe;
    setSP(stack);
    write_environ(argc, argv, envp);
    //hexdump(sp(), &ram[physicalAddress(sp(), SS, false)], stack-sp(), 0);
    //int extra = stack - sp();
    if (f_verbose)
        printf("Text %04x Data %04x Stack %04x\n", tseg, dseg+bseg+4096, 4096);

    for (int i=dseg; i<dseg+bseg; i++)  /* clear BSS */
        writeByte(0, i, DS);

    load_bios_irqs();
    set_entry_registers();
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
        if (f_verbose) printf("EXIT %d\n", bx());
        exit(bx());
    case 4:             // write
        p = &ram[physicalAddress(cx(), SS, false)];
        v = write(bx(), p, dx());
        setAX(v);
        break;
    case 5:             // open
        p = &ram[physicalAddress(bx(), SS, false)];
        printf("open '%s',%x,%o\n", p, cx(), dx());
        setAX(-2);
        break;
    case 54:            // ioctl
        if (f_verbose)
            printf("IOCTL %d,%c%02d,%x\n", bx(), cx()>>8, cx()&0xff, dx());
        setAX(bx() < 3? 0: -1);
        break;
    case 17:            // brk
        printf("BRK old %x new %x\n", sysbrk, bx());
        sysbrk = bx();
        //setShadowFlags on new break
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
