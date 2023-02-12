/*
 * ELKS symbol table support
 *
 * July 2022 Greg Haerr
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include "syms.h"

static unsigned char *syms;         // FIXME remove global
struct minix_exec_hdr sym_hdr;

#if __ia16__
#define ALLOC(s,n)    ((int)(s = sbrk(n)) != -1)
#else
#define ALLOC(s,n)    ((s = malloc(n)) !=  NULL)
char * __program_filename;
#endif

#define MAGIC       0x0301  /* magic number for executable progs */

/* read symbol table from executable into memory */
unsigned char * noinstrument sym_read_exe_symbols(char *path)
{
    int fd;
    unsigned char *s;

    if (syms) return syms;
    if ((fd = open(path, O_RDONLY)) < 0) {
#if __ia16__
        char fullpath[128];
        sprintf(fullpath, "/bin/%s", path);     // FIXME use PATH
        if ((fd = open(fullpath, O_RDONLY)) < 0)
#endif
                return NULL;
    }
    errno = 0;
    if (read(fd, &sym_hdr, sizeof(sym_hdr)) != sizeof(sym_hdr)
        || ((sym_hdr.type & 0xFFFF) != MAGIC)
        || sym_hdr.syms == 0
#if __ia16__
        || sym_hdr.syms > 32767
#endif
        || (!ALLOC(s, (int)sym_hdr.syms))
        || (lseek(fd, -(int)sym_hdr.syms, SEEK_END) < 0)
        || (read(fd, s, (int)sym_hdr.syms) != (int)sym_hdr.syms)) {
                int e = errno;
                close(fd);
                errno = e;
                return NULL;
    }
    close(fd);
    syms = s;
    return syms;
}

/* read symbol table file into memory */
unsigned char * noinstrument sym_read_symbols(char *path)
{
    int fd;
    unsigned char *s;
    struct stat sbuf;

    if (syms) return syms;
    if ((fd = open(path, O_RDONLY)) < 0)
        return NULL;
    errno = 0;
    if (fstat(fd, &sbuf) < 0
        || sbuf.st_size == 0
#if __ia16__
        || sbuf.st_size > 32767
#endif
        || (!ALLOC(s, (int)sbuf.st_size))
        || (read(fd, s, (int)sbuf.st_size) != (int)sbuf.st_size)) {
                int e = errno;
                close(fd);
                errno = e;
                return NULL;
    }
    close(fd);
    syms = s;
    return syms;
}

/* dealloate symbol table file in memory */
void noinstrument sym_free(void)
{
#ifndef __ia16__        // FIXME ELKS uses sbrk()
    if (syms)
        free(syms);
#endif
    syms = NULL;
}

static int noinstrument type_text(unsigned char *p)
{
    return (p[TYPE] == 'T' || p[TYPE] == 't' || p[TYPE] == 'W');
}

static int noinstrument type_ftext(unsigned char *p)
{
    return (p[TYPE] == 'F' || p[TYPE] == 'f');
}

static int noinstrument type_data(unsigned char *p)
{
    return (p[TYPE] == 'D' || p[TYPE] == 'd' ||
            p[TYPE] == 'B' || p[TYPE] == 'b' ||
            p[TYPE] == 'V');
}

/* map .text address to function start address */
addr_t  noinstrument sym_fn_start_address(addr_t addr) 
{
    unsigned char *p, *lastp;

    if (!syms && !sym_read_exe_symbols(__program_filename)) return -1;

    lastp = syms;
    for (p = next(lastp); ; lastp = p, p = next(p)) {
        if (!type_text(p) || ((unsigned short)addr < *(unsigned short *)(&p[ADDR])))
            break;
    }
    return *(unsigned short *)(&lastp[ADDR]);
}

/* convert address to symbol string */
static char * noinstrument sym_string(addr_t addr, int exact,
    int (*istype)(unsigned char *p))
{
    unsigned char *p, *lastp;
    static char buf[64];

    if (!syms && !sym_read_exe_symbols(__program_filename)) {
hex:
        sprintf(buf, "%.4x", (unsigned int)addr);
        return buf;
    }

    lastp = syms;
    while (!istype(lastp)) {
        lastp = next(lastp);
        if (!lastp[TYPE])
            goto hex;
    }
    for (p = next(lastp); ; lastp = p, p = next(p)) {
        if (!istype(p) || ((unsigned short)addr < *(unsigned short *)(&p[ADDR])))
            break;
    }
    int lastaddr = *(unsigned short *)(&lastp[ADDR]);
    if (exact && addr - lastaddr) {
        sprintf(buf, "%.*s+%xh", lastp[SYMLEN], lastp+SYMBOL,
                                (unsigned int)addr - lastaddr);
    } else sprintf(buf, "%.*s", lastp[SYMLEN], lastp+SYMBOL);
    return buf;
}

/* convert .text address to symbol */
char * noinstrument sym_text_symbol(addr_t addr, int exact)
{
    return sym_string(addr, exact, type_text);
}

/* convert .fartext address to symbol */
char * noinstrument sym_ftext_symbol(addr_t addr, int exact)
{
    return sym_string(addr, exact, type_ftext);
}

/* convert .data address to symbol */
char * noinstrument sym_data_symbol(addr_t addr, int exact)
{
    return sym_string(addr, exact, type_data);
}

#if 0
static int noinstrument type_any(unsigned char *p)
{
    return p[TYPE] != '\0';
}

/* convert (non-segmented local IP) address to symbol */
char * noinstrument sym_symbol(addr_t addr, int exact)
{
    return sym_string(addr, exact, type_any);
}
#endif
