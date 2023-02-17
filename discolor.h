#ifndef COLORINST_H_
#define COLORINST_H_
#include "disasm.h"

#if BLINK16
#include "blink/high.h"
#else
struct highlight {
  int enabled;
  int active;
  unsigned char keyword;
  unsigned char reg;
  unsigned char literal;
  unsigned char label;
  unsigned char comment;
  unsigned char quote;
  unsigned char grey;
  unsigned char symbol;
};

extern struct highlight g_high;
#endif

char *highStart(char *, int);
char *highEnd(char *);
char *colorInst(struct dis *d, char *str);

#endif /* COLORINST_H_ */
