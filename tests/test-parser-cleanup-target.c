/* Execute the actual parser cleanup routine from the complete product image. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned int symbol(const char* labels, const char* wanted)
{
    FILE* file = fopen(labels, "r");
    char line[256], name[128];
    unsigned int address;
    assert(file);
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "al %x %127s", &address, name) == 2 &&
            name[0] == '.' && strcmp(name + 1, wanted) == 0) {
            fclose(file);
            return address;
        }
    }
    abort();
}

int main(int argc, char** argv)
{
    static const unsigned char phrase[] = "go!,  north.  (now)";
    static const unsigned char cleaned[] = "go north now";
    static const unsigned char crossing[] = { 'a', ',', ' ', ' ', 'b', '!', 0xc1, 0 };
    static const unsigned char crossingCleaned[] = { 'a', ' ', 'b', 0xc1, 0 };
    FILE* input = tmpfile();
    FILE* output = tmpfile();
    FILE* bank;
    unsigned int routine, stack, size, address, i, repeat;
    unsigned long value, before = 0, elapsed = 0;
    char line[256];
    int status, byte, phase = 0, valid = 1;
    pid_t child;
    assert(argc == 4 && input && output);
    routine = symbol(argv[2], "_b7StripExtraChars");
    size = symbol(argv[2], "_b7StripExtraCharsEnd") - routine;
    stack = symbol(argv[2], "c_sp");
    bank = fopen(argv[3], "rb");
    assert(bank);
    address = (unsigned int)fgetc(bank);
    address |= (unsigned int)fgetc(bank) << 8;
    fprintf(input, "RAM 07\n");
    while ((byte = fgetc(bank)) != EOF) fprintf(input, "STM %04X %02X\n", address++, byte);
    fclose(bank);
    /* The C software stack is fixed RAM; the parser routine lives in bank 7. */
    fprintf(input, "RAM 00\nSTM %04X 00\nSTM %04X 8F\nRAM 07\nRQC\n", stack, stack + 1);
    for (repeat = 0; repeat < 32; repeat++) {
        for (i = 0; i < sizeof(phrase); i++) fprintf(input, "STM %04X %02X\n", 0x8000 + i, phrase[i]);
        fprintf(input, "STA 00\nSTX 80\nRUN %04X\n", routine);
    }
    fprintf(input, "RQC\n");
    for (i = 0; i < sizeof(cleaned); i++) fprintf(input, "RQM %04X\n", 0x8000 + i);
    for (i = 0; i < sizeof(crossing); i++) fprintf(input, "STM %04X %02X\n", 0x80fd + i, crossing[i]);
    fprintf(input, "STA FD\nSTX 80\nRUN %04X\n", routine);
    for (i = 0; i < sizeof(crossingCleaned); i++) fprintf(input, "RQM %04X\n", 0x80fd + i);
    rewind(input);
    child = fork();
    assert(child >= 0);
    if (!child) {
        assert(dup2(fileno(input), STDIN_FILENO) >= 0);
        assert(dup2(fileno(output), STDOUT_FILENO) >= 0);
        assert(dup2(fileno(output), STDERR_FILENO) >= 0);
        execlp("timeout", "timeout", "30", getenv("X16EMU"), "-testbench", "-rom",
               getenv("X16_ROM"), "-prg", argv[1], "-warp", "-sound", "none", (char*)NULL);
        _exit(127);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    rewind(output);
    while (fgets(line, sizeof(line), output)) {
        char extra;
        if (sscanf(line, "%lx %c", &value, &extra) != 1) continue;
        if (!phase) before = value;
        else if (phase == 1) elapsed = value - before;
        else {
            unsigned int offset = phase - 2;
            unsigned char expected;
            assert(offset < sizeof(cleaned) + sizeof(crossingCleaned));
            expected = offset < sizeof(cleaned) ? cleaned[offset] : crossingCleaned[offset - sizeof(cleaned)];
            if (value != expected) {
                fprintf(stderr, "parser byte %d: got %02lx, expected %02x\n",
                        phase - 2, value, expected);
                valid = 0;
            }
        }
        phase++;
    }
    assert(phase == 2 + sizeof(cleaned) + sizeof(crossingCleaned));
    printf("Parser cleanup: 32 phrases, %lu cycles, %u code bytes\n", elapsed, size);
    fflush(stdout);
    /* Same full-product workload: the original C routine took 511,232 cycles. */
    assert(elapsed < 90000);
    assert(size <= 96);
    assert(valid);
    fclose(input);
    fclose(output);
    return 0;
}
