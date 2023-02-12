/* ELKS symbol table support */

/* symbol table format
 *  | byte type | word address | byte symbol length | symbol |
 *  type: (lower case means static)
 *      T, t    .text
 *      F, f    .fartext
 *      D, d    .data
 *      B, b    .bss
 *      0       end of symbol table
 */

#define next(sym)   \
    ((sym) + 1 + sizeof(unsigned short) + ((unsigned char *)sym)[SYMLEN] + 1)
#define TYPE        0
#define ADDR        1
#define SYMLEN      3
#define SYMBOL      4

#ifndef noinstrument
#define noinstrument    __attribute__((no_instrument_function))
#endif

typedef unsigned int addr_t;    /* ELKS a.out address size (short) or larger */

unsigned char * noinstrument sym_read_exe_symbols(char *path);
unsigned char * noinstrument sym_read_symbols(char *path);
void noinstrument sym_free(void);
char * noinstrument sym_text_symbol(addr_t addr, int exact);
char * noinstrument sym_ftext_symbol(addr_t addr, int exact);
char * noinstrument sym_data_symbol(addr_t addr, int exact);
char * noinstrument sym_symbol(addr_t addr, int exact);
addr_t  noinstrument sym_fn_start_address(addr_t addr);

/* a.out header */
#include <stdint.h>

struct minix_exec_hdr {
    uint32_t  type;
    uint8_t   hlen;       // 0x04
    uint8_t   reserved1;
    uint16_t  version;
    uint32_t  tseg;       // 0x08
    uint32_t  dseg;       // 0x0c
    uint32_t  bseg;       // 0x10
    uint32_t  entry;
    uint16_t  chmem;
    uint16_t  minstack;
    uint32_t  syms;
};

extern struct minix_exec_hdr sym_hdr;
