/* loadexec-dos.c - MSDOS-specific functions for 8086 simulator */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "sim.h"

static char* pathBuffers[2];
static int* fileDescriptors;
static int fileDescriptorCount = 6;

static void write_environ(int argc, char **argv, char **envp)
{
    int envSegment = loadSegment - 0x1c;

    setES(envSegment);
    writeByte(0, 0, ES);  // No environment for now
    writeWord(1, 1, ES);
    int i;
    for (i = 0; filename[i] != 0; ++i)
        writeByte(filename[i], i + 3, ES);
    if (i + 4 >= 0xc0) {
        fprintf(stderr, "Program name too long.\n");
        exit(1);
    }
    writeWord(0, i + 3, ES);
    setES(loadSegment - 0x10);
    writeWord(envSegment, 0x2c, ES);
    i = 0x81;
    for (int a = 2; a < argc; ++a) {
        if (a > 2) {
            writeByte(' ', i, ES);
            ++i;
        }
        char* arg = argv[a];
        bool quote = strchr(arg, ' ') != 0;
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
        fprintf(stderr, "Arguments too long.\n");
        exit(1);
    }
    writeWord(0x9fff, 2, ES);
    writeByte(i - 0x81, 0x80, ES);
    writeByte(13, i, ES);
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

void load_executable(FILE *fp, int length, int argc, char **argv)
{
	init();

	ip = 0x100;
	flags = 2;
	write_environ(argc, argv, 0);
    for (int i = 0; i < length; ++i) {
        setES(loadSegment + (i >> 4));
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
        int exeLength = ((readWord(0x104) - (bytesInLastBlock == 0 ? 0 : 1)) << 9)
			+ bytesInLastBlock;
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
            setCS(readWord(relocationData + 0x102) + imageSegment);
            writeWord(readWordSeg(offset, CS) + imageSegment, offset, CS);
            relocationData += 4;
        }
        //loadSegment = imageSegment;  // Prevent further access to header
        Word ss = readWord(0x10e) + imageSegment;
        setSS(ss);
        setSP(readWord(0x110));
        stackLow = (((exeLength - headerLength + 15) >> 4) + imageSegment) << 4;
        if (stackLow < ((DWord)ss << 4) + 0x10)
            stackLow = ((DWord)ss << 4) + 0x10;
        ip = readWord(0x114);
        setCS(readWord(0x116) + imageSegment);
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
}

void set_entry_registers(void)
{
	if (!f_asmout)
		printf("CS:IP %x:%x DS %x SS:SP %x:%x\n", cs(), ip, ds(), ss(), sp());
    setES(loadSegment - 0x10);
    setAX(0x0000);
    setBX(0x0000);
    setCX(0x00FF);	// MUST BE 0x00FF as for big endian test below!!!!
    setDX(segment);
    setBP(0x091C);
    setSI(0x0100);
    setDI(0xFFFE);
}

void load_bios_irqs(void)
{
    // Fill up parts of the interrupt vector table, the BIOS clock tick count,
    // and parts of the BIOS ROM area with stuff, for the benefit of the far
    // pointer tests.
    setES(0x0000);
    writeWord(0x0000, 0x0080, ES);
    writeWord(0xFFFF, 0x0082, ES);
    writeWord(0x0058, 0x046C, ES);
    writeWord(0x000C, 0x046E, ES);
    writeByte(0x00, 0x0470, ES);
    setES(0xF000);
    for (int i = 0; i < 0x100; i += 2)
        writeWord(0xF4F4, 0xFF00 + (unsigned)i, ES);
    // We need some variety in the ROM BIOS content...
    writeByte(0xEA, 0xFFF0, ES);
    writeWord(0xFFF0, 0xFFF1, ES);
    writeWord(0xF000, 0xFFF3, ES);
}

char* initString(Word offset, int seg, bool write, int buffer, int bytes)
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
char* dsdxparms(bool write, int bytes)
{
    return initString(dx(), 3, write, 0, bytes);
}
char *dsdx()
{
	return dsdxparms(false, 0x10000);
}
int dosError(int e)
{
    if (e == ENOENT)
        return 2;
    fprintf(stderr, "%s\n", strerror(e));
    runtimeError("");
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

void handle_intcall(int intno)
{
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
                            initString(si(), 3, true, 0, 0x10000);
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
                        if (rename(dsdx(), initString(di(), 0, false, 1, 0x10000)) == 0)
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
                            "ah = 0x%02x", intno, (unsigned)ah());
                        runtimeError("");
						break;
                }
}
