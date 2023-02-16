/* loadexec-elks.c - ELKS-specific functions for 8086 simulator */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "8086.h"
#include "exe.h"

extern int f_verbose;

static int calc_environ(int argc, char **argv, char **envp)
{
    char **p;
    int argv_len=0, argv_count=0;
    int envp_len=0, envp_count=0;
    int stack_bytes;

    /* How much space for argv */
    for(p=argv; p && *p && argv_len >= 0; p++) {
        argv_count++;
        argv_len += strlen(*p)+1;
    }

    /* How much space for envp */
    for(p=envp; p && *p && envp_len >= 0; p++) {
        envp_count++;
        envp_len += strlen(*p)+1;
    }

    /* tot it all up */
    stack_bytes = 2             /* argc */
        + argv_count * 2 + 2    /* argv */
        + argv_len
        + envp_count * 2 + 2    /* envp */
        + envp_len;

    stack_bytes = (stack_bytes + 1) & ~1;
    return stack_bytes;
}

static void write_environ(int argc, char **argv, char **envp)
{
    char **p;
    int argv_len=0, argv_count=0;
    int envp_len=0, envp_count=0;
    int stack_bytes;
    int pip;
    int pcp, baseoff;

    /* How much space for argv */
    for(p=argv; p && *p && argv_len >= 0; p++) {
        argv_count++;
        argv_len += strlen(*p)+1;
    }

    /* How much space for envp */
    for(p=envp; p && *p && envp_len >= 0; p++) {
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
    for(p=argv; p && *p; p++) {
        writeWord(pcp-baseoff, pip, SS); pip += 2;
        int n = strlen(*p)+1;
        for (int i = 0; i<n; i++)
        writeByte((*p)[i], pcp+i, SS);
        pcp += n;
    }
    writeWord(0, pip, SS);  pip += 2;

    for(p=envp; p && *p; p++) {
        writeWord(pcp-baseoff, pip, SS); pip += 2;
        int n = strlen(*p)+1;
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
    Word loadSegment;
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
    if (e->aout.version != 1)
        loadError("Version 0 header programs not yet supported: %s\n", path);
    if (fstat(fd, &sbuf) < 0)
        loadError("Can't stat %s\n", path);
    size_t filesize = sbuf.st_size - e->aout.syms - e->aout.hlen;   /* text+data */

    loadSegment = 0x1000;
    int loadOffset = loadSegment << 4;
    if (filesize > RAMSIZE - loadOffset)
        loadError("Not enough memory to load %s, needs %d bytes have %d\n",
            path, filesize, RAMSIZE);
    if (read(fd, &ram[loadOffset], filesize) != filesize)
        loadError("Error reading executable: %s\n", path);
    close(fd);

    unsigned int tseg = e->aout.tseg;
    tseg = (tseg + 15) & ~15;       /* not strictly necessary */
    unsigned int dseg = e->aout.dseg;
    unsigned int bseg = e->aout.bseg;
    unsigned int stack = e->aout.minstack? e->aout.minstack: 0x1000;
    unsigned int slen = calc_environ(argc, argv, envp);
    unsigned int len = dseg + bseg + stack + slen;
    unsigned int heap = e->aout.chmem? e->aout.chmem: 0x1000;
    if (heap >= 0xFFF0) {           /* max heap specified */
        if (len < 0xFFF0)
            len = 0xFFF0;
    } else {
        len += heap;
    }
    len = (len + 15) & ~15;
    if (f_verbose)
        printf("tseg %04x dseg %04x bseg %04x heap %04x stack %04x totdata %04x\n",
            tseg, dseg, bseg, heap, stack, len);
    if (len > 0xFFFF)
        loadError("Program heap+stack >= 64K: %s\n", path);

    setES(loadSegment);
    setShadowFlags(0, ES, tseg, fRead); /* text read-only */
    setES(loadSegment + (tseg >> 4));
    setSS(es());                        /* SS just after text segment */
    setDS(ss());                        /* DS = SS */
    //FIXME don't allow stack reads before written
    //FIXME don't allow use of area outside break
    setShadowFlags(0, ES, len, fRead|fWrite);

    setCS(loadSegment);
    setIP(e->aout.entry & 0xffff);

    e->t_endseg = len;
    e->t_begstack = len - slen;
    e->t_minstack = stack;
    e->t_enddata = dseg + bseg;
    e->t_endbrk = e->t_enddata;     /* current break is end of data+bss */
    e->t_begstack &= ~1;            /* even SP */
    if (e->t_endbrk & 1)            /* even heap start */
        e->t_endbrk++;
    setSP(e->t_begstack);
    e->t_stackLow = (ss() << 4) + e->t_begstack - e->t_minstack;

    write_environ(argc, argv, envp);
    if (f_verbose)
        printf("Text %04x Data %04x Stack %04x\n", tseg, len-stack, stack);

    //hexdump(sp(), &ram[physicalAddress(sp(), SS, false)], stack-sp(), 0);
    //for (int i=dseg; i<dseg+bseg; i++)  /* clear BSS */
        //writeByte(0, i, DS);

    load_bios_irqs();
    set_entry_registers();
}

/* return true on stack overflow */
int checkStack(struct exe *e)
{
    return (e->t_stackLow && ((DWord)ss() << 4) + sp() < e->t_stackLow);
    //return (e->t_minstack && sp() < e->t_begstack - e->t_minstack);
    /* allow more than min stack down to break */
    //return (e->t_endbrk && sp() < e->t_endbrk);
}

static int SysExit(struct exe *e, int rc)
{
    if (f_verbose) printf("EXIT %d\n", rc);
    exit(rc);
    return -1;
}

static int SysWrite(struct exe *e, int fd, char *buf, size_t n)
{
    return write(fd, buf, n);
}

static int SysRead(struct exe *e, int fd, char *buf, size_t n)
{
    return read(fd, buf, n);
}

static int SysOpen(struct exe *e, char *path, int oflag, int mode)
{
    if (f_verbose)
        printf("[sys_open '%s',%d,%x]\n", path, oflag, mode);
    int ret = open(path, oflag, mode);
    if (ret < 0)
        printf("[sys_open failed: %s\n", path);
    return ret;
}

static int SysClose(struct exe *e, int fd)
{
    return close(fd);
}

static int SysBreak(struct exe *e, unsigned newbrk)
{
    if (f_verbose)
        printf("[sys_brk old %04x new %04x]\n", e->t_endbrk, newbrk);
    if (newbrk < e->t_enddata)
        return -ENOMEM;
    if (newbrk > e->t_begstack - e->t_minstack) {
        printf("sys_brk fail: brk %04x over by %u bytes\n",
                newbrk, newbrk - (e->t_begstack - e->t_minstack));
        return -ENOMEM;
    }
    e->t_endbrk = newbrk;
    return 0;
}

static int SysSbrk(struct exe *e, int incr, int offset_result)
{
    unsigned int brk = e->t_endbrk;     /* always return start of old break */
    int err;

    if (f_verbose)
        printf("[sys_sbrk %d old %04x new %04x SP %04x\n", incr, brk, brk+incr, sp());
    if (incr) {
        err = SysBreak(e, brk + incr);
        if (err) return err;
    }
    writeWord(brk, offset_result, SS);
    return 0;
}

#define CASE(OP, CODE) \
  case OP:             \
    CODE;              \
    break

#define SYSCALL(x, name, args)  \
  CASE(x, AX = name args )

#define rptr(off)     ((char *)&ram[physicalAddress(off, SS, false)])
#define wptr(off)     ((char *)&ram[physicalAddress(off, SS, true)])

void handleInterrupt(struct exe *e, int intno)
{
    unsigned int AX = ax();
    unsigned int BX = bx();
    unsigned int CX = cx();
    unsigned int DX = dx();

    switch (intno) {
    case INT0_DIV_ERROR:
        runtimeError("Divide by zero");
        return;
    case INT3_BREAKPOINT:
        runtimeError("Breakpoint trap");
        return;
    case INT4_OVERFLOW:
        runtimeError("Overflow trap");
        return;
    case 0x80:              /* sys call */
        break;
    default:
        runtimeError("Unknown INT 0x%02x", intno);
    }
    /* syscall args: BX, CX, DX, DI, SI */
    switch (AX) {
    SYSCALL(1,  SysExit,  (e, BX));
    SYSCALL(3,  SysRead,  (e, BX, rptr(CX), DX));
    SYSCALL(4,  SysWrite, (e, BX, rptr(CX), DX));
    SYSCALL(5,  SysOpen,  (e, rptr(BX), CX, DX));
    SYSCALL(6,  SysClose, (e, BX));
    SYSCALL(17, SysBreak, (e, BX));
    SYSCALL(69, SysSbrk,  (e, BX, CX));
    case 54:            // ioctl
        if (f_verbose)
            printf("IOCTL %d,%c%02d,%x\n", bx(), cx()>>8, cx()&0xff, dx());
        setAX(bx() < 3? 0: -1);
        return;
    default:
        fprintf(stderr, "Unknown SYS call: AX %04x(%d) BX %04x CX %04x DX %04x\n",
            AX, AX, BX, CX, DX);
        //runtimeError("");
        AX = -1;
    }
    setAX(AX);
}
