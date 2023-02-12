#ifndef COLORINST_H_
#define COLORINST_H_
#include "disasm.h"

struct High {
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

extern struct High g_high;

char *HighStart(char *, int);
char *HighEnd(char *);
char *ColorInst(struct dis *d, char *str);

#endif /* COLORINST_H_ */
