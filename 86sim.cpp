/*
 * 8086 emulator
 *
 * Orginally from Andrew Jenner's reenigne project
 * DOS enhancements by TK Chia
 * ELKS executable support by Greg Haerr
 * Disassembler added by Greg Haerr
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef unsigned char Byte;
typedef unsigned short int Word;
typedef unsigned int DWord;

Word registers[12];
Byte* byteRegisters[8];
Word ip;
Byte* ram;
Byte* initialized;
char* pathBuffers[2];
int* fileDescriptors;
int fileDescriptorCount = 6;
Word loadSegment;
bool useMemory;
Word address;
Word flags;
Byte modRM;
bool wordSize;
bool sourceIsRM;
DWord data;
DWord destination;
DWord source;
int segment = 0;
int rep = 0;
bool repeating = false;
Word savedIP;
Word savedCS;
int segmentOverride = -1;
Word residue;
Byte opcode;
int aluOperation;
const char* filename;
int length;
int ios;
bool running = false;
DWord stackLow;
int oCycle;

Word disasm(Word ip);
bool f_noaddr;

void o(char c)
{
#if 0
    while (oCycle < ios) {
        ++oCycle;
        printf(" ");
    }
    ++oCycle;
    printf("%c", c);
#endif
}

Word es() { return registers[8]; }
Word cs() { return registers[9]; }
void error(const char* operation)
{
    fprintf(stderr, "Error %s file %s: %s\n", operation, filename,
        strerror(errno));
    exit(1);
}
void runtimeError(const char* message)
{
    fprintf(stderr, "%s\nCS:IP = %04x:%04x\n", message, cs(), ip);
    exit(1);
}
void* alloc(size_t bytes)
{
    void* r = malloc(bytes);
    if (r == 0) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return r;
}
int getDescriptor()
{
    for (int i = 0; i < fileDescriptorCount; ++i)
        if (fileDescriptors[i] == -1)
            return i;
    int newCount = fileDescriptorCount << 1;
    int* newDescriptors = (int*)alloc(newCount*sizeof(int));
    for (int i = 0; i < fileDescriptorCount; ++i)
        newDescriptors[i] = fileDescriptors[i];
    free(fileDescriptors);
    int oldCount = fileDescriptorCount;
    fileDescriptorCount = newCount;
    fileDescriptors = newDescriptors;
    return oldCount;
}
void divideOverflow() { runtimeError("Divide overflow"); }
DWord physicalAddress(Word offset, int seg, bool write)
{
    ++ios;
    if (ios == 0)
        runtimeError("Cycle counter overflowed.");
    if (seg == -1) {
        seg = segment;
        if (segmentOverride != -1)
            seg = segmentOverride;
    }
    Word segmentAddress = registers[8 + seg];
    DWord a = (((DWord)segmentAddress << 4) + offset) & 0xfffff;
    bool bad = false;
    if (write) {
        if (a < ((DWord)loadSegment << 4) - 0x100 && running)
             bad = true;
        initialized[a >> 3] |= 1 << (a & 7);
    }
    if ((initialized[a >> 3] & (1 << (a & 7))) == 0 || bad) {
        fprintf(stderr, "Accessing invalid address %04x:%04x.\n",
            segmentAddress, offset);
#if MSDOS
        runtimeError("");
#endif
    }
    return a;
}
Byte readByte(Word offset, int seg = -1)
{
    return ram[physicalAddress(offset, seg, false)];
}
Word readWord(Word offset, int seg = -1)
{
    Word r = readByte(offset, seg);
    return r + (readByte(offset + 1, seg) << 8);
}
Word readwb(Word offset, int seg = -1)
{
    return wordSize ? readWord(offset, seg) : readByte(offset, seg);
}
void writeByte(Byte value, Word offset, int seg = -1)
{
    ram[physicalAddress(offset, seg, true)] = value;
}
void writeWord(Word value, Word offset, int seg = -1)
{
    writeByte((Byte)value, offset, seg);
    writeByte((Byte)(value >> 8), offset + 1, seg);
}
void writewb(Word value, Word offset, int seg = -1)
{
    if (wordSize)
        writeWord(value, offset, seg);
    else
        writeByte((Byte)value, offset, seg);
}
Byte fetchByte() { Byte b = readByte(ip, 1); ++ip; return b; }
Word fetchWord() { Word w = fetchByte(); w += fetchByte() << 8; return w; }
Word fetch(bool wordSize)
{
    if (wordSize)
        return fetchWord();
    return fetchByte();
}
Word signExtend(Byte data) { return data + (data < 0x80 ? 0 : 0xff00); }
int modRMReg() { return (modRM >> 3) & 7; }
void div()
{
    bool negative = false;
    bool dividendNegative = false;
    if (modRMReg() == 7) {
        if ((destination & 0x80000000) != 0) {
            destination = (unsigned)-(signed)destination;
            negative = !negative;
            dividendNegative = true;
        }
        if ((source & 0x8000) != 0) {
            source = (unsigned)-(signed)source & 0xffff;
            negative = !negative;
        }
    }
    data = destination / source;
    DWord product = data * source;
    // ISO C++ 2003 does not specify a rounding mode, but the x86 always
    // rounds towards zero.
    if (product > destination) {
        --data;
        product -= source;
    }
    residue = destination - product;
    if (negative)
        data = (unsigned)-(signed)data;
    if (dividendNegative)
        residue = (unsigned)-(signed)residue;
}
void doJump(Word newIP)
{
#if 0
    printf("\n");
#endif
    ip = newIP;
}
void jumpShort(Byte data, bool jump)
{
    if (jump)
        doJump(ip + signExtend(data));
}
void setCF(bool cf) { flags = (flags & ~1) | (cf ? 1 : 0); }
void setAF(bool af) { flags = (flags & ~0x10) | (af ? 0x10 : 0); }
void clearCA() { setCF(false); setAF(false); }
void setOF(bool of) { flags = (flags & ~0x800) | (of ? 0x800 : 0); }
void clearCAO() { clearCA(); setOF(false); }
void setPF()
{
    static Byte table[0x100] = {
        4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
        0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
        0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
        4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
        0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
        4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
        4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
        0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
        0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
        4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
        4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
        0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
        4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
        0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
        0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
        4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4};
    flags = (flags & ~4) | table[data & 0xff];
}
void setZF()
{
    flags = (flags & ~0x40) |
        ((data & (!wordSize ? 0xff : 0xffff)) == 0 ? 0x40 : 0);
}
void setSF()
{
    flags = (flags & ~0x80) |
        ((data & (!wordSize ? 0x80 : 0x8000)) != 0 ? 0x80 : 0);
}
void setPZS() { setPF(); setZF(); setSF(); }
void bitwise(Word value) { data = value; clearCAO(); setPZS(); }
void test(Word d, Word s)
{
    destination = d;
    source = s;
    bitwise(destination & source);
}
bool cf() { return (flags & 1) != 0; }
bool pf() { return (flags & 4) != 0; }
bool af() { return (flags & 0x10) != 0; }
bool zf() { return (flags & 0x40) != 0; }
bool sf() { return (flags & 0x80) != 0; }
void setIF(bool intf) { flags = (flags & ~0x200) | (intf ? 0x200 : 0); }
void setDF(bool df) { flags = (flags & ~0x400) | (df ? 0x400 : 0); }
bool df() { return (flags & 0x400) != 0; }
bool of() { return (flags & 0x800) != 0; }
Word rw() { return registers[opcode & 7]; }
Word ax() { return registers[0]; }
Word cx() { return registers[1]; }
Word dx() { return registers[2]; }
Word bx() { return registers[3]; }
Word sp() { return registers[4]; }
Word bp() { return registers[5]; }
Word si() { return registers[6]; }
Word di() { return registers[7]; }
Byte al() { return *byteRegisters[0]; }
Byte cl() { return *byteRegisters[1]; }
Byte dl() { return *byteRegisters[2]; }
Byte bl() { return *byteRegisters[3]; }
Byte ah() { return *byteRegisters[4]; }
Byte ch() { return *byteRegisters[5]; }
Byte dh() { return *byteRegisters[6]; }
Byte bh() { return *byteRegisters[7]; }
void setRW(Word value) { registers[opcode & 7] = value; }
void setAX(Word value) { registers[0] = value; }
void setCX(Word value) { registers[1] = value; }
void setDX(Word value) { registers[2] = value; }
void setBX(Word value) { registers[3] = value; }
void setSP(Word value) { registers[4] = value; }
void setSI(Word value) { registers[6] = value; }
void setDI(Word value) { registers[7] = value; }
void setAL(Byte value) { *byteRegisters[0] = value; }
void setCL(Byte value) { *byteRegisters[1] = value; }
void setAH(Byte value) { *byteRegisters[4] = value; }
void setRB(Byte value) { *byteRegisters[opcode & 7] = value; }
void setCS(Word value) { registers[9] = value; }
int stringIncrement()
{
    int r = (wordSize ? 2 : 1);
    return !df() ? r : -r;
}
Word lodS()
{
    address = si();
    setSI(si() + stringIncrement());
    segment = 3;
    return readwb(address);
}
void doRep(bool compare)
{
    if (rep == 1 && !compare)
        runtimeError("REPNE prefix with non-compare string instruction");
    if (rep == 0 || cx() == 0)
        return;
    setCX(cx() - 1);
    repeating = cx() != 0 && (!compare || zf() != (rep == 1));
}
Word lodDIS()
{
    address = di();
    setDI(di() + stringIncrement());
    return readwb(address, 0);
}
void stoS(Word data)
{
    address = di();
    setDI(di() + stringIncrement());
    writewb(data, address, 0);
}
void push(Word value)
{
    o('{');
    setSP(sp() - 2);
#if 0 // MSDOS FIXME - stack overflow with some DOS programs (test.exe)
    if (((DWord)registers[10] << 4) + sp() <= stackLow)
        runtimeError("Stack overflow");
#endif
    writeWord(value, sp(), 2);
}
Word pop() { Word r = readWord(sp(), 2); setSP(sp() + 2); o('}'); return r; }
void setCA() { setCF(true); setAF(true); }
void doAF() { setAF(((data ^ source ^ destination) & 0x10) != 0); }
void doCF() { setCF((data & (!wordSize ? 0x100 : 0x10000)) != 0); }
void setCAPZS() { setPZS(); doAF(); doCF(); }
void setOFAdd()
{
    Word t = (data ^ source) & (data ^ destination);
    setOF((t & (!wordSize ? 0x80 : 0x8000)) != 0);
}
void add() { data = destination + source; setCAPZS(); setOFAdd(); }
void setOFSub()
{
    Word t = (destination ^ source) & (data ^ destination);
    setOF((t & (!wordSize ? 0x80 : 0x8000)) != 0);
}
void sub() { data = destination - source; setCAPZS(); setOFSub(); }
void setOFRotate()
{
    setOF(((data ^ destination) & (!wordSize ? 0x80 : 0x8000)) != 0);
}
void doALUOperation()
{
    switch (aluOperation) {
        case 0: add(); o('+'); break;
        case 1: bitwise(destination | source); o('|'); break;
        case 2: source += cf() ? 1 : 0; add(); o('a'); break;
        case 3: source += cf() ? 1 : 0; sub(); o('B'); break;
        case 4: test(destination, source); o('&'); break;
        case 5: sub(); o('-'); break;
        case 7: sub(); o('?'); break;
        case 6: bitwise(destination ^ source); o('^'); break;
    }
}
Word* modRMRW() { return &registers[modRMReg()]; }
Byte* modRMRB() { return byteRegisters[modRMReg()]; }
Word getReg()
{
    if (!wordSize)
        return *modRMRB();
    return *modRMRW();
}
Word getAccum() { return !wordSize ? al() : ax(); }
void setAccum() { if (!wordSize) setAL(data); else setAX(data);  }
void setReg(Word value)
{
    if (!wordSize)
        *modRMRB() = (Byte)value;
    else
        *modRMRW() = value;
}
Word ea()
{
    modRM = fetchByte();
    useMemory = true;
    switch (modRM & 7) {
        case 0: segment = 3; address = bx() + si(); break;
        case 1: segment = 3; address = bx() + di(); break;
        case 2: segment = 2; address = bp() + si(); break;
        case 3: segment = 2; address = bp() + di(); break;
        case 4: segment = 3; address =        si(); break;
        case 5: segment = 3; address =        di(); break;
        case 6: segment = 2; address = bp();        break;
        case 7: segment = 3; address = bx();        break;
    }
    switch (modRM & 0xc0) {
        case 0x00:
            if ((modRM & 0xc7) == 6) {
                segment = 3;
                address = fetchWord();
            }
            break;
        case 0x40: address += signExtend(fetchByte()); break;
        case 0x80: address += fetchWord(); break;
        case 0xc0:
            useMemory = false;
            address = modRM & 7;
    }
    return address;
}
Word readEA2()
{
    if (!useMemory) {
        if (wordSize)
            return registers[address];
        return *byteRegisters[address];
    }
    return readwb(address);
}
Word readEA() { address = ea(); return readEA2(); }
void finishWriteEA(Word data)
{
    if (!useMemory) {
        if (wordSize)
            registers[address] = data;
        else
            *byteRegisters[address] = (Byte)data;
    }
    else
        writewb(data, address);
}
void writeEA(Word data) { ea(); finishWriteEA(data); }
void farLoad()
{
    if (!useMemory)
        runtimeError("This instruction needs a memory address");
    savedIP = readWord(address);
    savedCS = readWord(address + 2);
}
void farJump() { setCS(savedCS); doJump(savedIP); }
void farCall() { push(cs()); push(ip); farJump(); }
Word incdec(bool decrement)
{
    source = 1;
    if (!decrement) {
        data = destination + source;
        setOFAdd();
    }
    else {
        data = destination - source;
        setOFSub();
    }
    doAF();
    setPZS();
    return data;
}
void call(Word address) { push(ip); doJump(address); }

#if MSDOS
void
write_environ(int argc, char **argv, char **envp)
{
    int envSegment = loadSegment - 0x1c;
    registers[8] = envSegment;
    writeByte(0, 0);  // No environment for now
    writeWord(1, 1);
    int i;
    for (i = 0; filename[i] != 0; ++i)
        writeByte(filename[i], i + 3);
    if (i + 4 >= 0xc0) {
        fprintf(stderr, "Program name too long.\n");
        exit(1);
    }
    writeWord(0, i + 3);
    registers[8] = loadSegment - 0x10;
    writeWord(envSegment, 0x2c);
    i = 0x81;
    for (int a = 2; a < argc; ++a) {
        if (a > 2) {
            writeByte(' ', i);
            ++i;
        }
        char* arg = argv[a];
        bool quote = strchr(arg, ' ') != 0;
        if (quote) {
            writeByte('\"', i);
            ++i;
        }
        for (; *arg != 0; ++arg) {
            int c = *arg;
            if (c == '\"') {
                writeByte('\\', i);
                ++i;
            }
            writeByte(c, i);
            ++i;
        }
        if (quote) {
            writeByte('\"', i);
            ++i;
        }
    }
    if (i > 0xff) {
        fprintf(stderr, "Arguments too long.\n");
        exit(1);
    }
    writeWord(0x9fff, 2);
    writeByte(i - 0x81, 0x80);
    writeByte(13, i);
}

void
load_executable(FILE *fp, int length, int argc, char **argv)
{
	ip = 0x100;
	flags = 2;
	write_environ(argc, argv, 0);
    for (int i = 0; i < length; ++i) {
        registers[8] = loadSegment + (i >> 4);
        physicalAddress(i & 15, 0, true);
    }
    for (int i = 0; i < 4; ++i)
        registers[8 + i] = loadSegment - 0x10;
    if (length >= 2 && readWord(0x100) == 0x5a4d) {  // .exe file?
        if (length < 0x21) {
            fprintf(stderr, "%s is too short to be an .exe file\n", filename);
            exit(1);
        }
        Word bytesInLastBlock = readWord(0x102);
        int exeLength = ((readWord(0x104) - (bytesInLastBlock == 0 ? 0 : 1))
            << 9) + bytesInLastBlock;
        int headerParagraphs = readWord(0x108);
        int headerLength = headerParagraphs << 4;
        if (exeLength > length || headerLength > length ||
            headerLength > exeLength) {
            fprintf(stderr, "%s is corrupt\n", filename);
            exit(1);
        }
        int relocationCount = readWord(0x106);
        Word imageSegment = loadSegment + headerParagraphs;
        int relocationData = readWord(0x118);
        for (int i = 0; i < relocationCount; ++i) {
            int offset = readWord(relocationData + 0x100);
            registers[9] = readWord(relocationData + 0x102) + imageSegment;
            writeWord(readWord(offset, 1) + imageSegment, offset, 1);
            relocationData += 4;
        }
        //loadSegment = imageSegment;  // Prevent further access to header
        Word ss = readWord(0x10e) + imageSegment;  // SS
        registers[10] = ss;
        setSP(readWord(0x110));
        stackLow =
            (((exeLength - headerLength + 15) >> 4) + imageSegment) << 4;
        if (stackLow < ((DWord)ss << 4) + 0x10)
            stackLow = ((DWord)ss << 4) + 0x10;
        ip = readWord(0x114);
        registers[9] = readWord(0x116) + imageSegment;  // CS
    }
    else {
        if (length > 0xff00) {
            fprintf(stderr, "%s is too long to be a .com file\n", filename);
            exit(1);
        }
        setSP(0xFFFE);
        stackLow = ((DWord)loadSegment << 4) + length;
    }
    // Some testcases copy uninitialized stack data, so mark as initialized
    // any locations that could possibly be stack.
    if (sp()) {
        Word d = 0;
        if (((DWord)registers[10] << 4) < stackLow)
            d = stackLow - ((DWord)registers[10] << 4);
        while (d < sp()) {
            writeByte(0, d, 2);
            ++d;
        }
    } else {
        Word d = 0;
        if (((DWord)registers[10] << 4) < stackLow)
            d = stackLow - ((DWord)registers[10] << 4);
        do {
            writeByte(0, d, 2);
            ++d;
        } while (d != 0);
    }
}

void set_entry_registers(void)
{
    registers[8] = loadSegment - 0x10;
    setAX(0x0000);
    setCX(0x00FF);	// MUST BE 0x00FF as for big endian test below!!!!
    setDX(segment);
    registers[3] = 0x0000;  // BX
    registers[5] = 0x091C;  // BP
    setSI(0x0100);
    setDI(0xFFFE);
}

void
load_bios_irqs(void)
{
    // Fill up parts of the interrupt vector table, the BIOS clock tick count,
    // and parts of the BIOS ROM area with stuff, for the benefit of the far
    // pointer tests.
    registers[8] = 0x0000;
    writeWord(0x0000, 0x0080);
    writeWord(0xFFFF, 0x0082);
    writeWord(0x0058, 0x046C);
    writeWord(0x000C, 0x046E);
    writeByte(0x00, 0x0470);
    registers[8] = 0xF000;
    for (int i = 0; i < 0x100; i += 2)
        writeWord(0xF4F4, 0xFF00 + (unsigned)i);
    // We need some variety in the ROM BIOS content...
    writeByte(0xEA, 0xFFF0);
    writeWord(0xFFF0, 0xFFF1);
    writeWord(0xF000, 0xFFF3);
}

char* initString(Word offset, int seg, bool write, int buffer,
    int bytes = 0x10000)
{
    for (int i = 0; i < bytes; ++i) {
        char p;
        if (write) {
            p = pathBuffers[buffer][i];
            ram[physicalAddress(offset + i, seg, true)] = p;
        }
        else {
            p = ram[physicalAddress(offset + i, seg, false)];
            pathBuffers[buffer][i] = p;
        }
        if (p == 0 && bytes == 0x10000)
            break;
    }
    if (!write)
        pathBuffers[buffer][0xffff] = 0;
    return pathBuffers[buffer];
}
char* dsdx(bool write = false, int bytes = 0x10000)
{
    return initString(dx(), 3, write, 0, bytes);
}
int dosError(int e)
{
    if (e == ENOENT)
        return 2;
    fprintf(stderr, "%s\n", strerror(e));
    runtimeError("");
    return 0;
}
void handle_intcall(void)
{
        int fileDescriptor;
                data = fetchByte();
                switch (data << 8 | ah()) {
                    case 0x1a00:
                        data = registers[8];
                        registers[8] = 0;
                        setDX(readWord(0x046c, 0));
                        setCX(readWord(0x046e, 0));
                        setAL(readByte(0x0470, 0));
                        registers[8] = data;
                        break;
		    		case 0x2109:
						{
						char *p = strchr((char *)dsdx(), '$');
						if (p) write(STDOUT_FILENO, (char *)dsdx(), p-(char *)dsdx());
						}
						break;
                    case 0x2130:
                        setAX(0x1403);
                        setBX(0xff00);
                        setCX(0);
                        break;
                    case 0x2139:
                        if (mkdir(dsdx(), 0700) == 0)
                            setCF(false);
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x213a:
                        if (rmdir(dsdx()) == 0)
                            setCF(false);
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x213b:
                        if (chdir(dsdx()) == 0)
                            setCF(false);
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x213c:
                        fileDescriptor = creat(dsdx(), 0700);
                        if (fileDescriptor != -1) {
                            setCF(false);
                            int guestDescriptor = getDescriptor();
                            setAX(guestDescriptor);
                            fileDescriptors[guestDescriptor] = fileDescriptor;
                        }
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x213d:
                        fileDescriptor = open(dsdx(), al() & 3, 0700);
                        if (fileDescriptor != -1) {
                            setCF(false);
                            setAX(getDescriptor());
                            fileDescriptors[ax()] = fileDescriptor;
                        }
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x213e:
                        fileDescriptor = fileDescriptors[bx()];
                        if (fileDescriptor == -1) {
                            setCF(true);
                            setAX(6);  // Invalid handle
                            break;
                        }
                        if (fileDescriptor >= 5 &&
                            close(fileDescriptor) != 0) {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        else {
                            fileDescriptors[bx()] = -1;
                            setCF(false);
                        }
                        break;
                    case 0x213f:
                        fileDescriptor = fileDescriptors[bx()];
                        if (fileDescriptor == -1) {
                            setCF(true);
                            setAX(6);  // Invalid handle
                            break;
                        }
                        data = read(fileDescriptor, pathBuffers[0], cx());
                        dsdx(true, cx());
                        if (data == (DWord)-1) {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        else {
                            setCF(false);
                            setAX(data);
                        }
                        break;
                    case 0x2140:
                        fileDescriptor = fileDescriptors[bx()];
                        if (fileDescriptor == -1) {
                            setCF(true);
                            setAX(6);  // Invalid handle
                            break;
                        }
                        data = write(fileDescriptor, dsdx(false, cx()), cx());
                        if (data == (DWord)-1) {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        else {
                            setCF(false);
                            setAX(data);
                        }
                        break;
                    case 0x2141:
                        if (unlink(dsdx()) == 0)
                            setCF(false);
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x2142:
                        fileDescriptor = fileDescriptors[bx()];
                        if (fileDescriptor == -1) {
                            setCF(true);
                            setAX(6);  // Invalid handle
                            break;
                        }
                        data = lseek(fileDescriptor, (cx() << 16) + dx(),
                            al());
                        if (data != (DWord)-1) {
                            setCF(false);
                            setDX(data >> 16);
                            setAX(data);
                        }
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x2144:
                        if (al() != 0) {
                            fprintf(stderr, "Unknown IOCTL 0x%02x", al());
                            runtimeError("");
                        }
                        fileDescriptor = fileDescriptors[bx()];
                        if (fileDescriptor == -1) {
                            setCF(true);
                            setAX(6);  // Invalid handle
                            break;
                        }
                        data = isatty(fileDescriptor);
                        if (data == 1) {
                            setDX(0x80);
                            setCF(false);
                        }
                        else {
                            if (errno == ENOTTY) {
                                setDX(0);
                                setCF(false);
                            }
                            else {
                                setAX(dosError(errno));
                                setCF(true);
                            }
                        }
                        break;
                    case 0x2147:
                        if (getcwd(pathBuffers[0], 64) != 0) {
                            setCF(false);
                            initString(si(), 3, true, 0);
                        }
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x214a:
                        // Only allow attempts to "resize" the PSP segment,
                        // and check that CS:IP and SS:SP do not overshoot the
                        // segment end
                        if (es() == loadSegment - 0x10) {
                            DWord memEnd = (DWord)(es() + bx()) << 4;
                            if (physicalAddress(ip, 1, false) < memEnd &&
                                physicalAddress(sp() - 1, 2, true) < memEnd) {
                                setCF(false);
                                break;
                            }
                        }
                        fprintf(stderr, "Bad attempt to resize DOS memory "
                            "block: int 0x21, ah = 0x4a, bx = 0x%04x, "
                            "es = 0x%04x", (unsigned)bx(), (unsigned)es());
                        runtimeError("");
                        break;
                    case 0x214c:
                        printf("\n*** Bytes: %i\n", length);
                        printf("*** Cycles: %i\n", ios);
                        printf("*** EXIT code %i\n", al());
                        exit(0);
                        break;
                    case 0x2156:
                        if (rename(dsdx(), initString(di(), 0, false, 1)) == 0)
                            setCF(false);
                        else {
                            setCF(true);
                            setAX(dosError(errno));
                        }
                        break;
                    case 0x2157:
                        switch (al()) {
                            case 0x00:
                                fileDescriptor = fileDescriptors[bx()];
                                if (fileDescriptor == -1) {
                                    setCF(true);
                                    setAX(6);  // Invalid handle
                                    break;
                                }
                                setCX(0x0000); // Return a "reasonable" file
                                setDX(0x0021); // time and file date
                                setCF(false);
                                break;
                            default:
                                fprintf(stderr, "Unknown DOS call: int 0x21, "
                                    "ax = 0x%04x", (unsigned)ax());
                                runtimeError("");
                        }
                        break;
                    default:
                        fprintf(stderr, "Unknown DOS/BIOS call: int 0x%02x, "
                            "ah = 0x%02x", (unsigned)data, (unsigned)ah());
                        runtimeError("");
						break;
                }
}
#endif

#if ELKS
unsigned int sysbrk;

#define isprint(c) ((c) > ' ' && (c) <= '~')

static int lastnum[16] = {-1};
static int lastaddr = -1;

static void
printline(int address, int *num, char *chr, int count, int summary)
{
	int   j;

	if (lastaddr >= 0)
	{
		for (j = 0; j < count; j++)
			if (num[j] != lastnum[j])
				break;
		if (j == 16 && summary)
		{
			if (lastaddr + 16 == address)
			{
				printf("*\n");
			}
			return;
		}
	}

	lastaddr = address;
	printf("%04x:", address);
	for (j = 0; j < count; j++)
	{
		if (j == 8)
			putchar(' ');
		if (num[j] >= 0)
			printf(" %02x", num[j]);
		else
			printf("   ");
		lastnum[j] = num[j];
		num[j] = -1;
	}

	for (j=count; j < 16; j++)
	{
		if (j == 8)
			putchar(' ');
		printf("   ");
	}

	printf("  ");
	for (j = 0; j < count; j++)
		printf("%c", chr[j]);
	printf("\n");
}

void hexdump(int startoff, unsigned char *addr, int count, int summary)
{
	int offset;
	char buf[20];
	int num[16];

	for (offset = startoff; count > 0; count -= 16, offset += 16)
	{
		int j, ch;

		memset(buf, 0, 16);
		for (j = 0; j < 16; j++)
			num[j] = -1;
		for (j = 0; j < 16; j++)
		{
			ch = *addr++;

			num[j] = ch;
			if (isprint(ch) && ch < 0x80)
				buf[j] = ch;
			else
				buf[j] = '.';
		}
		printline(offset, num, buf, count > 16? 16: count, summary);
	}
}

void
write_environ(int argc, char **argv, char **envp)
{
	char **p;
	int argv_len=0, argv_count=0;
	int envp_len=0, envp_count=0;
	int stack_bytes;
	int pip;
	int pcp, baseoff;

	/* How much space for argv */
	for(p=argv; p && *p && argv_len >= 0; p++)
	{
	   //printf("arg %s\n", *p);
	   argv_count++; argv_len += strlen(*p)+1;
	}

	/* How much space for envp */
	for(p=envp; p && *p && envp_len >= 0; p++)
	{
	   //printf("env %s\n", *p);
	   envp_count++; envp_len += strlen(*p)+1;
	}

	/* tot it all up */
	stack_bytes = 2				/* argc */
	            + argv_count * 2 + 2	/* argv */
		    + argv_len
		    + envp_count * 2 + 2	/* envp */
		    + envp_len;

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
	writeWord(argv_count, pip, 2);	pip += 2;
	for(p=argv; p && *p; p++)
	{
	   int n;

	   writeWord(pcp-baseoff, pip, 2);	pip += 2;
	   //printf("argv = %d, ", pcp-baseoff);
	   n = strlen(*p)+1;
	   for (int i = 0; i<n; i++) {
		writeByte((*p)[i], pcp+i, 2);
		//printf("%c", (*p)[i]);
	   }
	   //printf("\n");
	   pcp += n;
	}
	writeWord(0, pip, 2);	pip += 2;

	for(p=envp; p && *p; p++)
	{
	   int n;

	   writeWord(pcp-baseoff, pip, 2);	pip += 2;
	   n = strlen(*p)+1;
	   for (int i = 0; i<n; i++)
		writeByte((*p)[i], pcp+i, 2);
	   pcp += n;
	}
	writeWord(0, pip, 2);	pip += 2;
}

void
load_executable(FILE *fp, int length, int argc, char **argv)
{
	extern char **environ;

	flags = 0x3202;
	// FIXME check hlen < 0x20, unset hdr access after, check tseg & 15
    for (int i = 0; i < 0x20; ++i) {
        registers[8] = loadSegment + (i >> 4); // ES
        physicalAddress(i & 15, 0, true);
    }
    // 8 = ES, 9 = CS, 10 = SS, 11 = DS YYY
    for (int i = 0; i < 4; ++i)
        registers[8 + i] = loadSegment;
    int hlen = readWord(0x04);
	int version = readWord(0x06);
	int tseg = readWord(0x08);
	int dseg = readWord(0x0C);
	int bseg = readWord(0x10);
	int entry = readWord(0x14);
	int chmem = readWord(0x18);
	int minstack = readWord(0x1C);
    printf("hlen %x version %x tseg %x dseg %x bseg %x entry %x chmem %x minstack %x\n",
		hlen, version, tseg, dseg, bseg, entry, chmem, minstack);
    for (int i = hlen; i < length+bseg+8192; ++i) {
        registers[8] = loadSegment + (i >> 4);
        physicalAddress(i & 15, 0, true); // ES
    }
    registers[9] = loadSegment + (hlen>>4); // CS
    registers[10] = loadSegment + (hlen>>4) + ((tseg + 15) >> 4); // SS
    registers[11] = registers[10]; // DS = SS
	sysbrk = dseg + bseg + 4096;
	int stack = sysbrk + 4096;
	//int stack = 0xfffe;
    setSP(stack);
	write_environ(argc, argv+1, environ);
	//hexdump(sp(), &ram[physicalAddress(sp(), 2, false)], stack-sp(), 0);
	//int extra = stack - sp();
	printf("Text %x Data %x Stack %x\n", tseg, dseg+bseg+4096, 4096);
	ip = entry;

	for (int i=dseg; i<dseg+bseg; i++)	// clear BSS
		writeByte(0, i, 3);		// DS:i
}

void set_entry_registers(void)
{
	printf("CS:IP %x:%x DS %x SS:SP %x:%x\n", registers[9], ip, registers[11],
					registers[10], sp());
    registers[8] = registers[11]; // ES = DS
    setAX(0x0000);
    setCX(0x00FF);	// MUST BE 0x00FF as for big endian test below!!!!
    setDX(0x0000);
    registers[3] = 0x0000;  // BX
    registers[5] = 0x0000;  // BP
    setSI(0x0000);
    setDI(0x0000);
}

void
load_bios_irqs(void)
{
}

void handle_intcall(void)
{
				unsigned char *p;
				unsigned int v;
                data = fetchByte();
				v = (data << 8) | ax();
                switch (v) {
				// ARGS: BX, CX, DX, DI, SI XXX
				case 0x8001:		// exit
					printf("EXIT %d\n", bx());
					exit(bx());
				case 0x8004:		// write
					//printf("WRITE %d,%x,%d\n", bx(), cx(), dx());
            		p = &ram[physicalAddress(cx(), 2, false)]; // SS
if (f_noaddr) v = 0; else
					v = write(bx(), p, dx());
					//printf("WRITE %d,%d,%d = %d\n", bx(), cx(), dx(), v);
					setAX(v);
					break;
				case 0x8005:		// open
            		p = &ram[physicalAddress(bx(), 2, false)]; // SS
					printf("open '%s',%x,%o\n", p, cx(), dx());
					setAX(-2);
					break;
				case 0x8036:		// ioctl
if (!f_noaddr)
					printf("IOCTL %d,%c%02d,%x\n", bx(), cx()>>8, cx()&0xff, dx());
					setAX(bx() < 3? 0: -1);
					break;
				case 0x8000+17:		// brk
					printf("BRK old %x new %x\n", sysbrk, bx());
					sysbrk = bx();
					setAX(0);
					break;
				case 0x8000+69:		// sbrk
					printf("SBRK %d old %x new %x SP %x\n", bx(), sysbrk, sysbrk+bx(), sp());
					v = sysbrk;
					sysbrk += bx();
					writeWord(v, cx(), 2); // SS
					setAX(0);
					break;
                default:
                    fprintf(stderr, "Unknown SYS call: int 0x%02x, "
                        "AX %x(%d) BX %x CX %x DX %x\n", (unsigned)data,
						ax(), ax(), bx(), cx(), dx());
                    //runtimeError("");
					setAX(0xffff);
					break;
                }
}
#endif

void emulator(void)
{
    Byte* byteData = (Byte*)&registers[0];
    int bigEndian = (byteData[2] == 0 ? 1 : 0);
    int byteNumbers[8] = {0, 2, 4, 6, 1, 3, 5, 7};
    for (int i = 0 ; i < 8; ++i)
        byteRegisters[i] = &byteData[byteNumbers[i] ^ bigEndian];

    running = true;
    bool prefix = false;
    for (int i = 0; i < 1000000000; ++i) {
        if (!repeating) {
			disasm(ip);
            if (!prefix) {
                segmentOverride = -1;
                rep = 0;
            }
            prefix = false;
            opcode = fetchByte();
        }
        if (rep != 0 && (opcode < 0xa4 || opcode >= 0xb0 || opcode == 0xa8 ||
            opcode == 0xa9))
            runtimeError("REP prefix with non-string instruction");
        wordSize = ((opcode & 1) != 0);
        sourceIsRM = ((opcode & 2) != 0);
        int operation = (opcode >> 3) & 7;
        bool jump;
        switch (opcode) {
            case 0x00: case 0x01: case 0x02: case 0x03:
            case 0x08: case 0x09: case 0x0a: case 0x0b:
            case 0x10: case 0x11: case 0x12: case 0x13:
            case 0x18: case 0x19: case 0x1a: case 0x1b:
            case 0x20: case 0x21: case 0x22: case 0x23:
            case 0x28: case 0x29: case 0x2a: case 0x2b:
            case 0x30: case 0x31: case 0x32: case 0x33:
            case 0x38: case 0x39: case 0x3a: case 0x3b:  // alu rmv,rmv
                data = readEA();
                if (!sourceIsRM) {
                    destination = data;
                    source = getReg();
                }
                else {
                    destination = getReg();
                    source = data;
                }
                aluOperation = operation;
                doALUOperation();
                if (aluOperation != 7) {
                    if (!sourceIsRM)
                        finishWriteEA(data);
                    else
                        setReg(data);
                }
                break;
            case 0x04: case 0x05: case 0x0c: case 0x0d:
            case 0x14: case 0x15: case 0x1c: case 0x1d:
            case 0x24: case 0x25: case 0x2c: case 0x2d:
            case 0x34: case 0x35: case 0x3c: case 0x3d:  // alu accum,i
                destination = getAccum();
                source = !wordSize ? fetchByte() : fetchWord();
                aluOperation = operation;
                doALUOperation();
                if (aluOperation != 7)
                    setAccum();
                break;
            case 0x06: case 0x0e: case 0x16: case 0x1e:  // PUSH segreg
                push(registers[operation + 8]);
                break;
            case 0x07: case 0x17: case 0x1f:  // POP segreg
                registers[operation + 8] = pop();
                break;
            case 0x26: case 0x2e: case 0x36: case 0x3e:  // segment override
                segmentOverride = operation - 4;
                o("e%ZE"[segmentOverride]);
                prefix = true;
                break;
            case 0x27: case 0x2f:  // DA
                if (af() || (al() & 0x0f) > 9) {
                    data = al() + (opcode == 0x27 ? 6 : -6);
                    setAL(data);
                    setAF(true);
                    if ((data & 0x100) != 0)
                        setCF(true);
                }
                setCF(cf() || al() > 0x9f);
                if (cf())
                    setAL(al() + (opcode == 0x27 ? 0x60 : -0x60));
                wordSize = false;
                data = al();
                setPZS();
                o(opcode == 0x27 ? 'y' : 'Y');
                break;
            case 0x37: case 0x3f:  // AA
                if (af() || (al() & 0xf) > 9) {
                    setAL(al() + (opcode == 0x37 ? 6 : -6));
                    setAH(ah() + (opcode == 0x37 ? 1 : -1));
                    setCA();
                }
                else
                    clearCA();
                setAL(al() & 0x0f);
                o(opcode == 0x37 ? 'A' : 'u');
                break;
            case 0x40: case 0x41: case 0x42: case 0x43:
            case 0x44: case 0x45: case 0x46: case 0x47:
            case 0x48: case 0x49: case 0x4a: case 0x4b:
            case 0x4c: case 0x4d: case 0x4e: case 0x4f:  // incdec rw
                destination = rw();
                wordSize = true;
                setRW(incdec((opcode & 8) != 0));
                o((opcode & 8) != 0 ? 'i' : 'd');
                break;
            case 0x50: case 0x51: case 0x52: case 0x53:
            case 0x54: case 0x55: case 0x56: case 0x57:  // PUSH rw
                push(rw());
                break;
            case 0x58: case 0x59: case 0x5a: case 0x5b:
            case 0x5c: case 0x5d: case 0x5e: case 0x5f:  // POP rw
                setRW(pop());
                break;
            case 0x60: case 0x61: case 0x62: case 0x63:
            case 0x64: case 0x65: case 0x66: case 0x67:
            case 0x68: case 0x69: case 0x6a: case 0x6b:
            case 0x6c: case 0x6d: case 0x6e: case 0x6f:
            case 0xc0: case 0xc1: case 0xc8: case 0xc9:  // invalid
            case 0xcc: case 0xf0: case 0xf1: case 0xf4:  // INT 3, LOCK, HLT
            case 0x9b: case 0xce: case 0x0f:  // WAIT, INTO, POP CS
            case 0xd8: case 0xd9: case 0xda: case 0xdb:
            case 0xdc: case 0xdd: case 0xde: case 0xdf:  // escape
            case 0xe4: case 0xe5: case 0xe6: case 0xe7:
            case 0xec: case 0xed: case 0xee: case 0xef:  // IN, OUT
                fprintf(stderr, "Invalid opcode %02x", opcode);
                runtimeError("");
                break;
            case 0x70: case 0x71: case 0x72: case 0x73:
            case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7a: case 0x7b:
            case 0x7c: case 0x7d: case 0x7e: case 0x7f:  // Jcond cb
                switch (opcode & 0x0e) {
                    case 0x00: jump = of(); break;
                    case 0x02: jump = cf(); break;
                    case 0x04: jump = zf(); break;
                    case 0x06: jump = cf() || zf(); break;
                    case 0x08: jump = sf(); break;
                    case 0x0a: jump = pf(); break;
                    case 0x0c: jump = sf() != of(); break;
                    default:   jump = sf() != of() || zf(); break;
                }
                jumpShort(fetchByte(), jump == ((opcode & 1) == 0));
                o("MK[)=J(]GgpP<.,>"[opcode & 0xf]);
                break;
            case 0x80: case 0x81: case 0x82: case 0x83:  // alu rmv,iv
                destination = readEA();
                data = fetch(opcode == 0x81);
                if (opcode != 0x83)
                    source = data;
                else
                    source = signExtend(data);
                aluOperation = modRMReg();
                doALUOperation();
                if (aluOperation != 7)
                    finishWriteEA(data);
                break;
            case 0x84: case 0x85:  // TEST rmv,rv
                data = readEA();
                test(data, getReg());
                o('t');
                break;
            case 0x86: case 0x87:  // XCHG rmv,rv
                data = readEA();
                finishWriteEA(getReg());
                setReg(data);
                o('x');
                break;
            case 0x88: case 0x89:  // MOV rmv,rv
                ea();
                finishWriteEA(getReg());
                o('m');
                break;
            case 0x8a: case 0x8b:  // MOV rv,rmv
                setReg(readEA());
                o('m');
                break;
            case 0x8c:  // MOV rmw,segreg
                ea();
                wordSize = 1;
                finishWriteEA(registers[modRMReg() + 8]);
                o('m');
                break;
            case 0x8d:  // LEA
                address = ea();
                if (!useMemory)
                    runtimeError("LEA needs a memory address");
                setReg(address);
                o('l');
                break;
            case 0x8e:  // MOV segreg,rmw
                wordSize = 1;
                data = readEA();
                registers[modRMReg() + 8] = data;
                o('m');
                break;
            case 0x8f:  // POP rmw
                writeEA(pop());
                break;
            case 0x90: case 0x91: case 0x92: case 0x93:
            case 0x94: case 0x95: case 0x96: case 0x97:  // XCHG AX,rw
                data = ax();
                setAX(rw());
                setRW(data);
                o(";xxxxxxx"[opcode & 7]);
                break;
            case 0x98:  // CBW
                setAX(signExtend(al()));
                o('b');
                break;
            case 0x99:  // CWD
                setDX((ax() & 0x8000) == 0 ? 0x0000 : 0xffff);
                o('w');
                break;
            case 0x9a:  // CALL cp
                savedIP = fetchWord();
                savedCS = fetchWord();
                o('c');
                farCall();
                break;
            case 0x9c:  // PUSHF
                o('U');
                push((flags & 0x0fd7) | 0xf000);
                break;
            case 0x9d:  // POPF
                o('O');
                flags = pop() | 2;
                break;
            case 0x9e:  // SAHF
                flags = (flags & 0xff02) | ah();
                o('s');
                break;
            case 0x9f:  // LAHF
                setAH(flags & 0xd7);
                o('L');
                break;
            case 0xa0: case 0xa1:  // MOV accum,xv
                segment = 3;
                data = readwb(fetchWord());
                setAccum();
                o('m');
                break;
            case 0xa2: case 0xa3:  // MOV xv,accum
                segment = 3;
                writewb(getAccum(), fetchWord());
                o('m');
                break;
            case 0xa4: case 0xa5:  // MOVSv
                if (rep == 0 || cx() != 0)
                    stoS(lodS());
                doRep(false);
                o('4' + (opcode & 1));
                break;
            case 0xa6: case 0xa7:  // CMPSv
                if (rep == 0 || cx() != 0) {
                    destination = lodS();
                    source = lodDIS();
                    sub();
                }
                doRep(true);
                o('0' + (opcode & 1));
                break;
            case 0xa8: case 0xa9:  // TEST accum,iv
                data = fetch(wordSize);
                test(getAccum(), data);
                o('t');
                break;
            case 0xaa: case 0xab:  // STOSv
                if (rep == 0 || cx() != 0)
                    stoS(getAccum());
                doRep(false);
                o('8' + (opcode & 1));
                break;
            case 0xac: case 0xad:  // LODSv
                if (rep == 0 || cx() != 0) {
                    data = lodS();
                    setAccum();
                }
                doRep(false);
                o('2' + (opcode & 1));
                break;
            case 0xae: case 0xaf:  // SCASv
                if (rep == 0 || cx() != 0) {
                    destination = getAccum();
                    source = lodDIS();
                    sub();
                }
                doRep(true);
                o('6' + (opcode & 1));
                break;
            case 0xb0: case 0xb1: case 0xb2: case 0xb3:
            case 0xb4: case 0xb5: case 0xb6: case 0xb7:
                setRB(fetchByte());
                o('m');
                break;
            case 0xb8: case 0xb9: case 0xba: case 0xbb:
            case 0xbc: case 0xbd: case 0xbe: case 0xbf:  // MOV rv,iv
                setRW(fetchWord());
                o('m');
                break;
            case 0xc2: case 0xc3: case 0xca: case 0xcb:  // RET
                savedIP = pop();
                savedCS = (opcode & 8) == 0 ? cs() : pop();
                if (!wordSize)
                    setSP(sp() + fetchWord());
                o('R');
                farJump();
                break;
            case 0xc4: case 0xc5:  // LES/LDS
                ea();
                farLoad();
                *modRMRW() = savedIP;
                registers[8 + (!wordSize ? 0 : 3)] = savedCS;
                o("NT"[opcode & 1]);
                break;
            case 0xc6: case 0xc7:  // MOV rmv,iv
                ea();
                finishWriteEA(fetch(wordSize));
                o('m');
                break;
            case 0xcd:
				handle_intcall();
                o('$');
                break;
            case 0xcf:  // IRET
                o('I');
                doJump(pop());
                setCS(pop());
                flags = pop() | 2;
                break;
            case 0xd0: case 0xd1: case 0xd2: case 0xd3:  // rot rmv,n
                data = readEA();
                if ((opcode & 2) == 0)
                    source = 1;
                else
                    source = cl();
                while (source != 0) {
                    destination = data;
                    switch (modRMReg()) {
                        case 0:  // ROL
                            data <<= 1;
                            doCF();
                            data |= (cf() ? 1 : 0);
                            setOFRotate();
                            break;
                        case 1:  // ROR
                            setCF((data & 1) != 0);
                            data >>= 1;
                            if (cf())
                                data |= (!wordSize ? 0x80 : 0x8000);
                            setOFRotate();
                            break;
                        case 2:  // RCL
                            data = (data << 1) | (cf() ? 1 : 0);
                            doCF();
                            setOFRotate();
                            break;
                        case 3:  // RCR
                            data >>= 1;
                            if (cf())
                                data |= (!wordSize ? 0x80 : 0x8000);
                            setCF((destination & 1) != 0);
                            setOFRotate();
                            break;
                        case 4:  // SHL
                        case 6:
                            data <<= 1;
                            doCF();
                            setOFRotate();
                            setPZS();
                            break;
                        case 5:  // SHR
                            setCF((data & 1) != 0);
                            data >>= 1;
                            setOFRotate();
                            setAF(true);
                            setPZS();
                            break;
                        case 7:  // SAR
                            setCF((data & 1) != 0);
                            data >>= 1;
                            if (!wordSize)
                                data |= (destination & 0x80);
                            else
                                data |= (destination & 0x8000);
                            setOFRotate();
                            setAF(true);
                            setPZS();
                            break;
                    }
                    --source;
                }
                finishWriteEA(data);
                o("hHfFvVvW"[modRMReg()]);
                break;
            case 0xd4:  // AAM
                data = fetchByte();
                if (data == 0)
                    divideOverflow();
                setAH(al() / data);
                setAL(al() % data);
                wordSize = true;
                setPZS();
                o('n');
                break;
            case 0xd5:  // AAD
                data = fetchByte();
                setAL(al() + ah()*data);
                setAH(0);
                setPZS();
                o('k');
                break;
            case 0xd6:  // SALC
                setAL(cf() ? 0xff : 0x00);
                o('S');
                break;
            case 0xd7:  // XLATB
                setAL(readByte(bx() + al()));
                o('@');
                break;
            case 0xe0: case 0xe1: case 0xe2:  // LOOPc cb
                setCX(cx() - 1);
                jump = (cx() != 0);
                switch (opcode) {
                    case 0xe0: if (zf()) jump = false; break;
                    case 0xe1: if (!zf()) jump = false; break;
                }
                o("Qqo"[opcode & 3]);
                jumpShort(fetchByte(), jump);
                break;
            case 0xe3:  // JCXZ cb
                o('z');
                jumpShort(fetchByte(), cx() == 0);
                break;
            case 0xe8:  // CALL cw
                data = fetchWord();
                o('c');
                call(ip + data);
                break;
            case 0xe9:  // JMP cw
                o('j');
                data = fetchWord();
                doJump(ip + data);
                break;
            case 0xea:  // JMP cp
                o('j');
                savedIP = fetchWord();
                savedCS = fetchWord();
                farJump();
                break;
            case 0xeb:  // JMP cb
                o('j');
                jumpShort(fetchByte(), true);
                break;
            case 0xf2: case 0xf3:  // REP
                o('r');
                rep = opcode == 0xf2 ? 1 : 2;
                prefix = true;
                break;
            case 0xf5:  // CMC
                o('\"');
                flags ^= 1;
                break;
            case 0xf6: case 0xf7:  // math rmv
                data = readEA();
                switch (modRMReg()) {
                    case 0: case 1:  // TEST rmv,iv
                        test(data, fetch(wordSize));
                        o('t');
                        break;
                    case 2:  // NOT iv
                        finishWriteEA(~data);
                        o('~');
                        break;
                    case 3:  // NEG iv
                        source = data;
                        destination = 0;
                        sub();
                        finishWriteEA(data);
                        o('_');
                        break;
                    case 4: case 5:  // MUL rmv, IMUL rmv
                        source = data;
                        destination = getAccum();
                        data = destination;
                        setSF();
                        setPF();
                        data *= source;
                        setAX(data);
                        if (!wordSize) {
                            if (modRMReg() == 4)
                                setCF(ah() != 0);
                            else {
                                if ((source & 0x80) != 0)
                                    setAH(ah() - destination);
                                if ((destination & 0x80) != 0)
                                    setAH(ah() - source);
                                setCF(ah() ==
                                    ((al() & 0x80) == 0 ? 0 : 0xff));
                            }
                        }
                        else {
                            setDX(data >> 16);
                            if (modRMReg() == 4) {
                                data |= dx();
                                setCF(dx() != 0);
                            }
                            else {
                                if ((source & 0x8000) != 0)
                                    setDX(dx() - destination);
                                if ((destination & 0x8000) != 0)
                                    setDX(dx() - source);
                                data |= dx();
                                setCF(dx() ==
                                    ((ax() & 0x8000) == 0 ? 0 : 0xffff));
                            }
                        }
                        setZF();
                        setOF(cf());
                        o("*#"[opcode & 1]);
                        break;
                    case 6: case 7:  // DIV rmv, IDIV rmv
                        source = data;
                        if (source == 0)
                            divideOverflow();
                        if (!wordSize) {
                            destination = ax();
                            if (modRMReg() == 6) {
                                div();
                                if (data > 0xff)
                                    divideOverflow();
                            }
                            else {
                                destination = ax();
                                if ((destination & 0x8000) != 0)
                                    destination |= 0xffff0000;
                                source = signExtend(source);
                                div();
                                if (data > 0x7f && data < 0xffffff80)
                                    divideOverflow();
                            }
                            setAH((Byte)residue);
                            setAL(data);
                        }
                        else {
                            destination = (dx() << 16) + ax();
                            div();
                            if (modRMReg() == 6) {
                                if (data > 0xffff)
                                    divideOverflow();
                            }
                            else {
                                if (data > 0x7fff && data < 0xffff8000)
                                    divideOverflow();
                            }
                            setDX(residue);
                            setAX(data);
                        }
                        o("/\\"[opcode & 1]);
                        break;
                }
                break;
            case 0xf8: case 0xf9:  // STC/CLC
                setCF(wordSize);
                o("\'`"[opcode & 1]);
                break;
            case 0xfa: case 0xfb:  // STI/CLI
                setIF(wordSize);
                o("!:"[opcode & 1]);
                break;
            case 0xfc: case 0xfd:  // STD/CLD
                setDF(wordSize);
                o("CD"[opcode & 1]);
                break;
            case 0xfe: case 0xff:  // misc
                ea();
                if ((!wordSize && modRMReg() >= 2 && modRMReg() <= 6) ||
                    modRMReg() == 7) {
                    fprintf(stderr, "Invalid instruction %02x %02x", opcode,
                        modRM);
                    runtimeError("");
                }
                switch (modRMReg()) {
                    case 0: case 1:  // incdec rmv
                        destination = readEA2();
                        finishWriteEA(incdec(modRMReg() != 0));
                        o("id"[modRMReg() & 1]);
                        break;
                    case 2:  // CALL rmv
                        o('c');
                        call(readEA2());
                        break;
                    case 3:  // CALL mp
                        o('c');
                        farLoad();
                        farCall();
                        break;
                    case 4:  // JMP rmw
                        o('j');
                        doJump(readEA2());
                        break;
                    case 5:  // JMP mp
                        o('j');
                        farLoad();
                        farJump();
                        break;
                    case 6:  // PUSH rmw
                        push(readEA2());
                        break;
                }
                break;
        }
    }
    runtimeError("Timed out");
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s <program name>\n", argv[0]);
        exit(0);
    }
    ram = (Byte*)alloc(0x100000);
    initialized = (Byte*)alloc(0x20000);
    pathBuffers[0] = (char*)alloc(0x10000);
    pathBuffers[1] = (char*)alloc(0x10000);
    memset(ram, 0, 0x100000);
    memset(initialized, 0, 0x20000);

    filename = argv[1];
    FILE* fp = fopen(filename, "rb");
    if (fp == 0)
        error("opening");
    if (fseek(fp, 0, SEEK_END) != 0)
        error("seeking");
    length = ftell(fp);
    if (length == -1)
        error("telling");
    if (fseek(fp, 0, SEEK_SET) != 0)
        error("seeking");
#if ELKS
	loadSegment = 0x1000 - 2;
#endif
#if MSDOS
	loadSegment = 0x0212;
#endif
    int loadOffset = loadSegment << 4;
    if (length > 0x100000 - loadOffset)
        length = 0x100000 - loadOffset;
    if (fread(&ram[loadOffset], length, 1, fp) != 1)
        error("reading");
    fclose(fp);

	load_executable(fp, length, argc, argv);
	load_bios_irqs();
	set_entry_registers();

    fileDescriptors = (int*)alloc(6*sizeof(int));
    fileDescriptors[0] = STDIN_FILENO;
    fileDescriptors[1] = STDOUT_FILENO;
    fileDescriptors[2] = STDERR_FILENO;
    fileDescriptors[3] = STDOUT_FILENO;
    fileDescriptors[4] = STDOUT_FILENO;
    fileDescriptors[5] = -1;

#ifdef __APPLE__	/* macOS stdio drops characters! */
    static char buf[1];
    setvbuf(stdout, buf, _IOFBF, sizeof(buf));
#endif

	emulator();
}
