/* ELKS disassembler header file */

/* to be defined by caller of disasm() */
char * getsymbol(int seg, int offset);
char * getsegsymbol(int seg);

/* disasm.c */
int disasm(int cs, int ip, int (*nextbyte)(int, int), int ds);
int nextbyte_mem(int cs, int ip);

extern int f_asmout;    /* =1 for asm output (no addresses or hex values) */
extern int f_outcol;    /* output column number (if !f_asmout) */
