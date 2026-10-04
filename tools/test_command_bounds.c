/* Exercise production message lookup with logic metadata supplied by its bank boundary. */
#define _CX16_H
#define asm(...) ((void)0)
#include <stdint.h>
#include "../src/commands.h"
#include <cx16.h>
#undef RAM_BANK
static byte test_ram_bank;
#define RAM_BANK test_ram_bank
#include "../src/commands.c"

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s\n", m); exit(EXIT_FAILURE); } } while (0)
static byte message[] = "message";
static byte *messages[] = { message };
static byte *full_messages[255];
static LOGICFile loaded_logic;
static boolean test_flags[256];
boolean *flag = test_flags;
static unsigned operand_reads, lookup_calls;
static byte operands[2] = { 7, 7 };
static unsigned comparison_calls;
char string[12][40];
static byte test_vars[256];
byte *var = test_vars;
static unsigned allocation_calls, deallocation_calls, input_calls;
static boolean allocation_failure = TRUE;
static byte input_allocation[80];

byte *b10BankedAlloc(int size, byte *bank)
{
    CHECK(size == 80, "get.num requested an unexpected input allocation");
    ++allocation_calls;
    *bank = 0x35;
    return allocation_failure ? NULL : input_allocation;
}
boolean b10BankedDealloc(byte *pointer, byte bank)
{
    CHECK(pointer == input_allocation && bank == 0x35,
          "get.num released the wrong allocation or bank");
    ++deallocation_calls;
    return TRUE;
}
void b7GetString(char *prompt, byte prompt_bank, char *destination, byte destination_bank,
                int x, int y, int length)
{
    (void)prompt; (void)prompt_bank; (void)destination_bank;
    (void)x; (void)y; (void)length;
    ++input_calls;
    CHECK(destination != NULL, "get.num passed a failed allocation to the input routine");
    strcpy(destination, "123");
}

byte loadAndIncWinCode(void)
{
    CHECK(operand_reads < 2, "Command read too many operands");
    return operands[operand_reads++];
}
int b7Strcmp(const char *left, const char *right)
{
    ++comparison_calls;
    return strcmp(left, right);
}
char *b7GetInternalStringPtr(byte number, size_t *length)
{
    CHECK(operand_reads == 1 && number == 7,
          "parse accessed an internal string before decoding its operand");
    *length = strlen((char *)message);
    return (char *)message;
}
void b7LookupWords(char *text)
{
    CHECK(text == (char *)message, "parse submitted the wrong string to the dictionary");
    ++lookup_calls;
}

void b5GetLogicFile(LOGICFile *file, byte number)
{
    (void)number;
    *file = loaded_logic;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--allocation")) {
        memset(&loaded_logic, 0, sizeof(loaded_logic));
        loaded_logic.numMessages = 1;
        loaded_logic.messages = messages;
        loaded_logic.messageBank = 5;
        test_ram_bank = 0x37;
        test_vars[0] = 0xA5;
        operands[0] = 1;
        operands[1] = 0;
        b4Get_num();
        CHECK(operand_reads == 2 && allocation_calls == 1 && !deallocation_calls && !input_calls,
              "get.num failed-allocation cleanup or operand consumption was incorrect");
        CHECK(test_vars[0] == 0xA5 && test_ram_bank == 0x37,
              "get.num failure changed a variable or the caller bank");
        operand_reads = 0;
        operands[0] = 0;
        b4Get_num();
        CHECK(operand_reads == 2 && allocation_calls == 1 && !input_calls,
              "get.num invalid message changed operand alignment or allocated input");
        operand_reads = 0;
        operands[0] = 1;
        allocation_failure = FALSE;
        b4Get_num();
        CHECK(operand_reads == 2 && allocation_calls == 2 && deallocation_calls == 1 && input_calls == 1,
              "get.num retry did not release its successful input allocation exactly once");
        CHECK(test_vars[0] == 123 && test_ram_bank == 0x37,
              "get.num retry did not publish the entered number");
        puts("Command input allocation failure passes");
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--strings")) {
        strcpy(string[0], "different");
        strcpy(string[11], "last");
        operands[0] = operands[1] = 11;
        CHECK(b1Compare_strings(), "Final internal string was rejected");
        CHECK(operand_reads == 2 && comparison_calls == 1,
              "String comparison did not consume both operands");
        operand_reads = comparison_calls = 0;
        operands[0] = 12;
        operands[1] = 0;
        CHECK(!b1Compare_strings() && operand_reads == 2 && !comparison_calls,
              "Invalid internal string reached the comparison helper");
        operand_reads = comparison_calls = 0;
        operands[0] = 0;
        operands[1] = 255;
        CHECK(!b1Compare_strings() && operand_reads == 2 && !comparison_calls,
              "Invalid second internal string reached the comparison helper");
        puts("Command internal-string bounds pass");
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--parse")) {
        test_flags[2] = test_flags[4] = TRUE;
        b4Parse();
        CHECK(operand_reads == 1 && lookup_calls == 1,
              "parse did not consume one operand and submit one string");
        CHECK(!test_flags[2] && !test_flags[4], "parse retained prior input flags");
        puts("Command parse operand order passes");
        return 0;
    }
    memset(&loaded_logic, 0, sizeof(loaded_logic));
    loaded_logic.numMessages = 1;
    loaded_logic.messages = messages;
    loaded_logic.messageBank = 5;
    test_ram_bank = 0x37;
    CHECK(getMessagePointer(0, 0) == (char *)message,
          "Valid final message was rejected");
    CHECK(test_ram_bank == 0x37, "Message lookup changed the caller bank");
    messages[0] = NULL;
    CHECK(getMessagePointer(0, 0) == NULL && test_ram_bank == 0x37,
          "Absent message slot was accepted or changed the caller bank");
    messages[0] = message;
    CHECK(getMessagePointer(0, 1) == NULL,
          "Message lookup accepted an index beyond the loaded message count");
    CHECK(test_ram_bank == 0x37, "Rejected message lookup changed the caller bank");
    CHECK(getMessagePointer(0, 255) == NULL,
          "Underflowed one-based message operand was accepted");
    loaded_logic.numMessages = 0;
    loaded_logic.messages = NULL;
    CHECK(getMessagePointer(0, 0) == NULL,
          "Empty logic message table was dereferenced");
    loaded_logic.numMessages = 1;
    CHECK(getMessagePointer(0, 0) == NULL,
          "Absent logic message table was dereferenced");
    for (unsigned i = 0; i < 255; ++i) full_messages[i] = message;
    loaded_logic.numMessages = 255;
    loaded_logic.messages = full_messages;
    CHECK(getMessagePointer(0, 254) == (char *)message,
          "Final message in a full byte-count table was rejected");
    CHECK(getMessagePointer(0, 255) == NULL,
          "Message 255 read beyond a full byte-count table");
    CHECK(test_ram_bank == 0x37, "Boundary lookups changed the caller bank");
    puts("Command message bounds pass");
    return 0;
}
