/* ELKS disassembler header file */

#ifndef noinstrument
#define noinstrument    __attribute__((no_instrument_function))
#endif

/* to be defined by caller of disasm() */
char * noinstrument getsymbol(int seg, int offset);
char * noinstrument getsegsymbol(int seg);

struct dis {
    unsigned int cs;
    unsigned int ip;
    unsigned int ds;
    unsigned int flags;
    unsigned int oplen;
    unsigned int col;
    int (*getbyte)(int, int);
    char *s;
    char buf[64];
};

/* disassembler flags */
#define fDisCSIP        0x0001  /* show address as CS:IP */
#define fDisAddr        0x0002  /* show linear address */
#define fDisBytes       0x0004  /* show byte codes */
#define fDisInst        0x0008  /* show instruction */
#define fDisAsmSource   0x0010  /* output gnu compatible 'as' input */

/* disasm.c */
// use unsigned!
int disasm(struct dis *d, int cs, int ip, int (*nextbyte)(int, int), int ds, int flags);
