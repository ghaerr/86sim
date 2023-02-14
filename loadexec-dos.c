/* loadexec-dos.c - MSDOS-specific functions for 8086 simulator */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "sim.h"
#include "exe.h"

/* loader globals */
Word loadSegment;
DWord stackLow;
extern int f_verbose;

static const char* filename;
static int filesize;
static char* pathBuffers[2];
static int* fileDescriptors;
static int fileDescriptorCount = 6;

static void write_environ(int argc, char **argv, char **envp)
{
    int envSegment = loadSegment - 0x1c;
    int i;

    setES(envSegment);
    setShadowFlags(0, ES, 0x100, fRead);
    writeByte(0, 0, ES);  // No environment for now
    writeWord(1, 1, ES);
    for (i = 0; filename[i] != 0; ++i)
        writeByte(filename[i], i + 3, ES);
    if (i + 4 >= 0xc0) {
        fprintf(stderr, "Program name too long.\n");    // FIXME
        exit(1);
    }
    writeWord(0, i + 3, ES);
    setES(loadSegment - 0x10);
    setShadowFlags(0, ES, 0x0100, fRead);
    writeWord(envSegment, 0x2c, ES);
    i = 0x81;
    for (int a = 2; a < argc; ++a) {
        if (a > 2) {
            writeByte(' ', i, ES);
            ++i;
        }
        char* arg = argv[a];
        int quote = strchr(arg, ' ') != 0;
        if (quote) {
            writeByte('\"', i, ES);
            ++i;
        }
        for (; *arg != 0; ++arg) {
            int c = *arg;
            if (c == '\"') {
                writeByte('\\', i, ES);
                ++i;
            }
            writeByte(c, i, ES);
            ++i;
        }
        if (quote) {
            writeByte('\"', i, ES);
            ++i;
        }
    }
    if (i > 0xff) {
        fprintf(stderr, "Arguments too long.\n");   // FIXME
        exit(1);
    }
    writeWord(0x9fff, 2, ES);
    writeByte(i - 0x81, 0x80, ES);
    writeByte('\r', i, ES);
}

static void* alloc(size_t bytes)
{
    void* r = malloc(bytes);
    if (r == 0) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    return r;
}

static void init()
{
    pathBuffers[0] = (char*)alloc(0x10000);
    pathBuffers[1] = (char*)alloc(0x10000);

    fileDescriptors = (int*)alloc(6*sizeof(int));
    fileDescriptors[0] = STDIN_FILENO;
    fileDescriptors[1] = STDOUT_FILENO;
    fileDescriptors[2] = STDERR_FILENO;
    fileDescriptors[3] = STDOUT_FILENO;
    fileDescriptors[4] = STDOUT_FILENO;
    fileDescriptors[5] = -1;

}

static void error(const char* operation)
{
    fprintf(stderr, "Error %s file %s: %s\n", operation, filename, strerror(errno));
    exit(1);
}

static void set_entry_registers(void)
{
    if (f_verbose)
        printf("CS:IP %04x:%04x DS %04x SS:SP %04x:%04x\n", cs(), getIP(), ds(), ss(), sp());
    setES(loadSegment - 0x10);
    setAX(0x0000);
    setBX(0x0000);
    setCX(0x0000);
    setDX(0x0000);  // was segment
    setBP(0x091C);
    setSI(0x0100);
    setDI(0xFFFE);
    setFlags(0x0002);
}

static void load_bios_irqs(void)
{
    // Fill up parts of the interrupt vector table, the BIOS clock tick count,
    // and parts of the BIOS ROM area with stuff, for the benefit of the far
    // pointer tests.
    setES(0x0000);
    setShadowFlags(0x0080, ES, 0x0004, fRead);
    writeWord(0x0000, 0x0080, ES);
    writeWord(0xFFFF, 0x0082, ES);
    setShadowFlags(0x0400, ES, 0x00FF, fRead);
    writeWord(0x0058, 0x046C, ES);
    writeWord(0x000C, 0x046E, ES);
    writeByte(0x00, 0x0470, ES);
    setES(0xF000);
    setShadowFlags(0xFF00, ES, 0x0100, fRead);
    for (int i = 0; i < 0x100; i += 2)
        writeWord(0xF4F4, 0xFF00 + i, ES);
    // We need some variety in the ROM BIOS content...
    writeByte(0xEA, 0xFFF0, ES);
    writeWord(0xFFF0, 0xFFF1, ES);
    writeWord(0xF000, 0xFFF3, ES);
}

void load_executable(struct exe *e, const char *path, int argc, char **argv, char **envp)
{
    init();

    filename = path;
    FILE* fp = fopen(filename, "rb");
    if (fp == 0)
        error("opening");
    if (fseek(fp, 0, SEEK_END) != 0)
        error("seeking");
    filesize = ftell(fp);
    if (filesize == -1)
        error("telling");
    if (fseek(fp, 0, SEEK_SET) != 0)
        error("seeking");

    loadSegment = 0x0212;
    int loadOffset = loadSegment << 4;
    if (filesize > 0x100000 - loadOffset)
        filesize = 0x100000 - loadOffset;
    if (fread(&ram[loadOffset], filesize, 1, fp) != 1)
        error("reading");
    fclose(fp);

    setIP(0x0100);
    setES(loadSegment);
    setShadowFlags(0, ES, filesize, fRead|fWrite);
    write_environ(argc, argv, envp);
    setES(loadSegment - 0x10);
    if (filesize >= 2 && readWordSeg(0x100, ES) == 0x5a4d) {  // .exe file?
        if (filesize < 0x21) {
            fprintf(stderr, "%s is too short to be an .exe file\n", filename);
            exit(1);
        }
        Word bytesInLastBlock = readWordSeg(0x102, ES);
        int exeLength = ((readWordSeg(0x104, ES) - (bytesInLastBlock == 0 ? 0 : 1)) << 9)
            + bytesInLastBlock;
        int headerParagraphs = readWordSeg(0x108, ES);
        int headerLength = headerParagraphs << 4;
        if (exeLength > filesize || headerLength > filesize ||
            headerLength > exeLength) {
            fprintf(stderr, "%s is corrupt\n", filename);   // FIXME
            exit(1);
        }
        int relocationCount = readWordSeg(0x106, ES);
        Word imageSegment = loadSegment + headerParagraphs;
        int relocationData = readWordSeg(0x118, ES);
        for (int i = 0; i < relocationCount; ++i) {
            int offset = readWordSeg(relocationData + 0x100, ES);
            setCS(readWordSeg(relocationData + 0x102, ES) + imageSegment);
            writeWord(readWordSeg(offset, CS) + imageSegment, offset, CS);
            relocationData += 4;
        }
        //loadSegment = imageSegment;  // Prevent further access to header
        Word ss = readWordSeg(0x10e, ES) + imageSegment;
        setSS(ss);
        setSP(readWordSeg(0x110, ES));
        stackLow = (((exeLength - headerLength + 15) >> 4) + imageSegment) << 4;
        if (stackLow < ((DWord)ss << 4) + 0x10)
            stackLow = ((DWord)ss << 4) + 0x10;
        setDS(loadSegment - 0x10);
        setIP(readWordSeg(0x114, ES));
        setCS(readWordSeg(0x116, ES) + imageSegment);
    }
    else {
        if (filesize > 0xff00) {
            fprintf(stderr, "%s is too long to be a .com file\n", filename);
            exit(1);
        }
        setCS(loadSegment);
        setDS(loadSegment);
        setSS(loadSegment);
        setSP(0xFFFE);
        stackLow = ((DWord)loadSegment << 4) + filesize;
    }
    // Some testcases copy uninitialized stack data, so mark as initialized
    // any locations that could possibly be stack.
    //if (a < ((DWord)loadSegment << 4) - 0x100 && running)
         //bad = true;
    setShadowFlags(0, SS, 0x10000, fRead|fWrite);    // FIXME only usable stack
#if 0
    if (sp()) {
        Word d = 0;
        if (((DWord)ss() << 4) < stackLow)
            d = stackLow - ((DWord)ss() << 4);
        while (d < sp()) {
            writeByte(0, d, SS);
            ++d;
        }
    } else {
        Word d = 0;
        if (((DWord)ss() << 4) < stackLow)
            d = stackLow - ((DWord)ss() << 4);
        do {
            writeByte(0, d, SS);
            ++d;
        } while (d != 0);
    }
#endif
    load_bios_irqs();
    set_entry_registers();
}

char* initString(Word offset, int seg, int write, int buffer, int bytes)
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
char* dsdxparms(int write, int bytes)
{
    return initString(dx(), DS, write, 0, bytes);
}
char *dsdx()
{
    return dsdxparms(false, 0x10000);
}
int dosError(int e)
{
    if (e == ENOENT)
        return 2;
    runtimeError("%s\n", strerror(e));
    return 0;
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

void handle_intcall(void *m, int intno)
{
        //struct exe *e = m;
        int fileDescriptor;
        char *p;
        DWord data;
                switch (intno << 8 | ah()) {
                    case 0x1a00:
                        data = es();
                        setES(0);
                        setDX(readWordSeg(0x046c, ES));
                        setCX(readWordSeg(0x046e, ES));
                        setAL(readByte(0x0470, ES));
                        setES(data);
                        break;
                    case 0x2109:
                        p = strchr((char *)dsdx(), '$');
                        if (p) write(STDOUT_FILENO, (char *)dsdx(), p-(char *)dsdx());
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
                        dsdxparms(true, cx());
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
                        data = write(fileDescriptor, dsdxparms(false, cx()), cx());
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
                        if (al() != 0)
                            runtimeError("Unknown IOCTL 0x%02x", al());
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
                            initString(si(), DS, true, 0, 0x10000);
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
                            if (physicalAddress(getIP(), CS, false) < memEnd &&
                                physicalAddress(sp() - 1, SS, true) < memEnd) {
                                setCF(false);
                                break;
                            }
                        }
                        runtimeError("Bad attempt to resize DOS memory "
                            "block: int 0x21, ah = 0x4a, bx = 0x%04x, "
                            "es = 0x%04x", (unsigned)bx(), (unsigned)es());
                        break;
                    case 0x214c:
                        if (f_verbose) {
                            printf("\n*** Bytes: %i\n", filesize);
                            //printf("*** Cycles: %i\n", ios);
                            printf("*** EXIT code %i\n", al());
                        }
                        exit(0);
                        break;
                    case 0x2156:
                        if (rename(dsdx(), initString(di(), ES, false, 1, 0x10000)) == 0)
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
                                runtimeError("Unknown DOS call: int 0x21, "
                                    "ax = 0x%04x", (unsigned)ax());
                        }
                        break;
                    default:
                        runtimeError("Unknown DOS/BIOS call: int 0x%02x, "
                            "ah = 0x%02x", intno, (unsigned)ah());
                        break;
                }
}
