/* sim.h */
typedef unsigned char Byte;
typedef unsigned short int Word;
typedef unsigned int DWord;
typedef int bool;
enum { false = 0, true };

/* loadexec-xxx.c entry points */
void load_executable(FILE *fp, int length, int argc, char **argv);
void load_bios_irqs(void);
void set_entry_registers(void);
void handle_intcall(int intno);

/* segment registers after 8 general registers */
enum { ES = 0, CS, SS, DS };

/* global simulator vars and functions */
extern Word registers[12];
extern Byte* byteRegisters[8];
extern Word flags;
extern Word ip;
extern Byte* ram;

extern Byte opcode;
extern int segment;
extern Word loadSegment;
extern const char* filename;
extern DWord stackLow;
extern int length;  // FIXME remove
extern int ios;

extern int f_disasm;      /* do disassembly */
extern int f_asmout;      /* output gnu as compatible input */

int initMachine(void);
void *alloc(size_t bytes);
void emulator(void);
void divideOverflow(void);
void runtimeError(const char* message);

Byte readByte(Word offset, int seg);
Word readWord(Word offset);
Word readWordSeg(Word offset, int seg);
void writeByte(Byte value, Word offset, int seg);
void writeWord(Word value, Word offset, int seg);
DWord physicalAddress(Word offset, int seg, bool write);

/* access functions */
static inline Word ax() { return registers[0]; }
static inline Word cx() { return registers[1]; }
static inline Word dx() { return registers[2]; }
static inline Word bx() { return registers[3]; }
static inline Word sp() { return registers[4]; }
static inline Word bp() { return registers[5]; }
static inline Word si() { return registers[6]; }
static inline Word di() { return registers[7]; }
static inline Word es() { return registers[8]; }
static inline Word cs() { return registers[9]; }
static inline Word ss() { return registers[10]; }
static inline Word ds() { return registers[11]; }
static inline Word rw() { return registers[opcode & 7]; }
static inline Byte al() { return *byteRegisters[0]; }
static inline Byte cl() { return *byteRegisters[1]; }
static inline Byte dl() { return *byteRegisters[2]; }
static inline Byte bl() { return *byteRegisters[3]; }
static inline Byte ah() { return *byteRegisters[4]; }
static inline Byte ch() { return *byteRegisters[5]; }
static inline Byte dh() { return *byteRegisters[6]; }
static inline Byte bh() { return *byteRegisters[7]; }
static inline void setAX(Word value) { registers[0] = value; }
static inline void setCX(Word value) { registers[1] = value; }
static inline void setDX(Word value) { registers[2] = value; }
static inline void setBX(Word value) { registers[3] = value; }
static inline void setSP(Word value) { registers[4] = value; }
static inline void setBP(Word value) { registers[5] = value; }
static inline void setSI(Word value) { registers[6] = value; }
static inline void setDI(Word value) { registers[7] = value; }
static inline void setES(Word value) { registers[8] = value; }
static inline void setCS(Word value) { registers[9] = value; }
static inline void setSS(Word value) { registers[10] = value; }
static inline void setDS(Word value) { registers[11] = value; }
static inline void setRW(Word value) { registers[opcode & 7] = value; }
static inline void setRB(Byte value) { *byteRegisters[opcode & 7] = value; }
static inline void setAL(Byte value) { *byteRegisters[0] = value; }
static inline void setCL(Byte value) { *byteRegisters[1] = value; }
static inline void setDL(Byte value) { *byteRegisters[2] = value; }
static inline void setBL(Byte value) { *byteRegisters[3] = value; }
static inline void setAH(Byte value) { *byteRegisters[4] = value; }
static inline void setCH(Byte value) { *byteRegisters[5] = value; }
static inline void setDH(Byte value) { *byteRegisters[6] = value; }
static inline void setBH(Byte value) { *byteRegisters[7] = value; }
static inline void setCF(bool cf) { flags = (flags & ~1) | (cf ? 1 : 0); }
