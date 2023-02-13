#ifndef EXE_H_
#define EXE_H_
/* ELKS a.out and DOS MZ headers */

#include <stdint.h>

/* minimal ELKS header */
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

/* ELKS optional fields */
struct elks_supl_hdr {
    uint32_t  msh_trsize;       /* text relocation size */      // 0x20
    uint32_t  msh_drsize;       /* data relocation size */      // 0x24
    uint32_t  msh_tbase;        /* text relocation base */
    uint32_t  msh_dbase;        /* data relocation base */
    uint32_t  esh_ftseg;        /* far text size */             // 0x30
    uint32_t  esh_ftrsize;      /* far text relocation size */  // 0x34
    uint16_t  esh_compr_tseg;   /* compressed tseg size */
    uint16_t  esh_compr_dseg;   /* compressed dseg size* */
    uint16_t  esh_compr_ftseg;  /* compressed ftseg size*/
    uint16_t  esh_reserved;
};

struct minix_reloc {
    uint32_t  r_vaddr;          /* address of place within section */
    uint16_t  r_symndx;         /* index into symbol table */   // 0x04
    uint16_t  r_type;           /* relocation type */           // 0x06
};

struct exe {
    struct minix_exec_hdr aout;
    struct elks_supl_hdr  eshdr;
    unsigned char *       syms;
    uint16_t              textseg;
    uint16_t              ftextseg;
    uint16_t              dataseg;
};

#define ELKSMAGIC   0x0301      /* magic number for ELKS executable progs */

/* loader entry points */
void load_executable(struct exe *e, const char *filename, int argc, char **argv, char **envp);

#endif /* EXE_H_ */
