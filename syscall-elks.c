/*
 * ELKS system calls for 8086 emulator
 *
 * Greg Haerr
 */
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

#if BLINK16
#include "machine.h"
#endif

extern int f_verbose;

/* return true on stack overflow */
int checkStackElks(struct exe *e)
{
    return (e->t_stackLow && ((DWord)ss() << 4) + sp() < e->t_stackLow);
    //return (e->t_minstack && sp() < e->t_begstack - e->t_minstack);
    /* allow more than min stack down to break */
    //return (e->t_endbrk && sp() < e->t_endbrk);
}

static int SysExit(struct exe *e, int rc)
{
#if BLINK16
    extern void ReactiveDraw(void);
    ReactiveDraw();
#else
    if (f_verbose) printf("EXIT %d\n", rc);
#endif
    exit(rc);
    return -1;
}

static int SysWrite(struct exe *e, int fd, char *buf, size_t n)
{
#if BLINK16
    extern ssize_t ptyWrite(int fd, char *buf, int len);
    extern struct Machine m;        // FIXME rewrite
    SetWriteAddr(&m, buf-(char *)ram, n);
    return ptyWrite(fd, buf, n);
#else
    return write(fd, buf, n);
#endif
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

void handleInterruptElks(struct exe *e, int intno)
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
