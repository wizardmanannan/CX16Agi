/* Execute compiled production C callers with per-bank 6502 memory. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "registers.h"
#include "fake6502.h"

extern struct regs regs;

#define BANK_START 0xA000u
#define BANK_END   0xC000u
#define BANK_SIZE  (BANK_END - BANK_START)
#define MEMORY_MANAGEMENT_BANK 0x10u
#define SOUND_BANK 0x0Bu
#define GETIN_ADDRESS 0xFFE4u
#define CBM_SETLFS_ADDRESS 0xFFBAu
#define CBM_SETNAM_ADDRESS 0xFFBDu
#define CBM_OPEN_ADDRESS  0xFFC0u
#define VERA_ADDR_LOW 0x9F20u
#define VERA_ADDR_HIGH 0x9F21u
#define VERA_ADDR_BANK 0x9F22u
#define VERA_DATA0 0x9F23u
#define PARSER_BANK 0x07u
#define INPUT_STRING_ADDRESS 0xE680u
#define RETURN_PC  0x0200u
#define CALLER_BANK 0x37u
#define STRING_BANK 0x1Bu

static uint8_t ram[65536];
static uint8_t bank_ram[256][BANK_SIZE];
static uint8_t image[65536];
static uint8_t text_code_image[BANK_SIZE];
static uint8_t irq_code_image[BANK_SIZE];
static uint8_t parser_code_image[BANK_SIZE];
static uint8_t commands_code_image[BANK_SIZE];
static uint8_t logic_code_image[BANK_SIZE];
static uint8_t memory_code_image[BANK_SIZE];
static uint8_t view_code_image[BANK_SIZE];
static uint8_t sound_code_image[BANK_SIZE];
static uint8_t vram[0x20000];
static uint32_t vera_address;
static unsigned vera_stride;
static bool vera_decrement;
static unsigned vera_data0_writes;
static const unsigned vera_increments[16] = { 0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 40, 80, 160, 320, 640 };
static struct { uint32_t address; uint8_t value; } vera_data0_history[16];
static unsigned vera_data0_history_count;
static bool capture_vera_data0_history;
static struct { char name[100]; uint16_t address; } labels[4096];
static unsigned label_count;
static unsigned bank_window_reads;
static unsigned out_of_window_reads;
static bool track_source_reads;
static uint8_t getin_keys[16];
static unsigned getin_count;
static unsigned getin_index;
static bool getin_enabled;
static unsigned cbm_open_calls;

#define CHECK(condition, message) do { \
	if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } \
} while (0)

static void advance_vera_address(void)
{
	if (vera_decrement)
		vera_address = (vera_address - vera_stride) & 0x1FFFFu;
	else
		vera_address = (vera_address + vera_stride) & 0x1FFFFu;
}

uint8_t read6502(uint16_t address, uint8_t bank)
{
	(void)bank;
	if (getin_enabled && address == GETIN_ADDRESS)
	{
		CHECK(getin_index < getin_count, "Production GETIN requested more keys than its input fixture supplied");
		regs.a = getin_keys[getin_index++];
		return 0x60; /* RTS: the intercepted KERNAL GETIN hardware call. */
	}
	if (address == CBM_SETLFS_ADDRESS || address == CBM_SETNAM_ADDRESS)
	{
		return 0x60; /* The metadata fixture supplies the KERNAL file-open seam. */
	}
	if (address == CBM_OPEN_ADDRESS)
	{
		++cbm_open_calls;
		regs.a = 1; /* Return a missing-file error so the real resolver stays empty. */
		regs.status |= FLAG_CARRY;
		return 0x60;
	}
	if (address == VERA_DATA0)
	{
		uint8_t value = vram[vera_address & 0x1FFFFu];
		advance_vera_address();
		return value;
	}
	if (track_source_reads && address >= BANK_END) ++out_of_window_reads;
	if (address >= BANK_START && address < BANK_END)
	{
		if (track_source_reads) ++bank_window_reads;
		return bank_ram[ram[0]][address - BANK_START];
	}
	return ram[address];
}

void write6502(uint16_t address, uint8_t bank, uint8_t value)
{
	(void)bank;
	if (address == VERA_ADDR_LOW)
	{
		ram[address] = value;
		vera_address = (vera_address & 0x1FF00u) | value;
		return;
	}
	if (address == VERA_ADDR_HIGH)
	{
		ram[address] = value;
		vera_address = (vera_address & 0x100FFu) | ((uint32_t)value << 8);
		return;
	}
	if (address == VERA_ADDR_BANK)
	{
		ram[address] = value;
		vera_address = (vera_address & 0x0FFFFu) | ((uint32_t)(value & 1) << 16);
		vera_stride = vera_increments[value >> 4];
		vera_decrement = (value & 8) != 0;
		return;
	}
	if (address == VERA_DATA0)
	{
		vram[vera_address & 0x1FFFFu] = value;
		++vera_data0_writes;
		if (capture_vera_data0_history)
		{
			CHECK(vera_data0_history_count < sizeof(vera_data0_history), "Sound playback wrote too many PSG values");
			vera_data0_history[vera_data0_history_count].address = vera_address & 0x1FFFFu;
			vera_data0_history[vera_data0_history_count++].value = value;
		}
		advance_vera_address();
		return;
	}
	if (address >= BANK_START && address < BANK_END)
	{
		bank_ram[ram[0]][address - BANK_START] = value;
	}
	else
	{
		ram[address] = value;
	}
}

uint8_t memory_get_ram_bank(void) { return ram[0]; }
uint8_t memory_get_rom_bank(void) { return ram[1]; }
void vp6502(void) {}
void stop6502(uint16_t address, uint8_t bank)
{
	(void)address;
	(void)bank;
	CHECK(false, "Banked string fixture stopped the CPU");
}

static uint16_t symbol(const char *name)
{
	unsigned i;
	for (i = 0; i < label_count; ++i)
	{
		if (!strcmp(name, labels[i].name)) return labels[i].address;
	}
	fprintf(stderr, "Missing production symbol %s\n", name);
	exit(1);
}

static void path(char *destination, size_t size, const char *build, const char *name)
{
	CHECK(snprintf(destination, size, "%s/%s", build, name) < (int)size, "Build path too long");
}

static void load_low_bank_image(const char *build, const char *name, uint8_t *destination,
	const char *description)
{
	char filename[4096];
	int low, high;
	FILE *file;
	size_t loaded;
	path(filename, sizeof(filename), build, name);
	file = fopen(filename, "rb");
	CHECK(file, description);
	low = fgetc(file);
	high = fgetc(file);
	CHECK(low >= 0 && high >= 0 && ((unsigned)low | ((unsigned)high << 8)) == BANK_START,
		"Banked code image has an invalid load address");
	memset(destination, 0xA5, BANK_SIZE);
	loaded = fread(destination, 1, BANK_SIZE, file);
	CHECK(loaded && !ferror(file), "Banked code image is empty or unreadable");
	CHECK(fgetc(file) == EOF && !ferror(file), "Banked code image exceeds bank window");
	CHECK(!fclose(file), "Cannot close banked code image");
}

static void load_raw_bank_image(const char *build, const char *name, uint8_t *destination,
	const char *description)
{
	char filename[4096];
	FILE *file;
	size_t loaded;
	path(filename, sizeof(filename), build, name);
	file = fopen(filename, "rb");
	CHECK(file, description);
	memset(destination, 0xA5, BANK_SIZE);
	loaded = fread(destination, 1, BANK_SIZE, file);
	CHECK(loaded && !ferror(file), "Raw bank image is empty or unreadable");
	CHECK(fgetc(file) == EOF && !ferror(file), "Raw bank image exceeds bank window");
	CHECK(!fclose(file), "Cannot close raw bank image");
}

static void load_image(const char *build)
{
	char filename[4096], line[256], kind[16], name[100];
	unsigned address;
	FILE *file;
	size_t loaded;

	path(filename, sizeof(filename), build, "final.lbl");
	file = fopen(filename, "r");
	CHECK(file, "Cannot open final.lbl");
	while (fgets(line, sizeof(line), file))
	{
		if (sscanf(line, "%15s %x %99s", kind, &address, name) != 3) continue;
		CHECK(label_count < sizeof(labels) / sizeof(labels[0]) && address < 65536, "Invalid label file");
		strcpy(labels[label_count].name, name[0] == '.' ? name + 1 : name);
		labels[label_count++].address = (uint16_t)address;
	}
	CHECK(!fclose(file), "Cannot close final.lbl");

	path(filename, sizeof(filename), build, "agi.cx16");
	file = fopen(filename, "rb");
	CHECK(file, "Cannot open resident image");
	int low = fgetc(file), high = fgetc(file);
	CHECK(low >= 0 && high >= 0, "Missing resident image load address");
	unsigned start = (unsigned)low | ((unsigned)high << 8);
	loaded = fread(image + start, 1, sizeof(image) - start, file);
	CHECK(loaded && feof(file) && !ferror(file), "Resident image too large or unreadable");
	CHECK(!fclose(file), "Cannot close resident image");

	load_low_bank_image(build, "agi.cx16.03", text_code_image, "Cannot open text code bank image");
	load_low_bank_image(build, "agi.cx16.04", commands_code_image, "Cannot open command code bank image");
	load_low_bank_image(build, "agi.cx16.05", logic_code_image, "Cannot open logic code bank image");
	load_low_bank_image(build, "agi.cx16.06", irq_code_image, "Cannot open IRQ code bank image");
	load_low_bank_image(build, "agi.cx16.07", parser_code_image, "Cannot open parser code bank image");
	load_raw_bank_image(build, "agi.cx16.10", memory_code_image, "Cannot open memory-manager code bank image");
	load_raw_bank_image(build, "agi.cx16.0E", view_code_image, "Cannot open view metadata code bank image");
	load_raw_bank_image(build, "agi.cx16.0B", sound_code_image, "Cannot open sound code bank image");
}

static void put_word(uint16_t address, uint16_t value)
{
	ram[address] = (uint8_t)value;
	ram[address + 1] = (uint8_t)(value >> 8);
}

static uint16_t get_word(uint16_t address)
{
	return (uint16_t)(ram[address] | ((uint16_t)ram[address + 1] << 8));
}

static void prepare(void)
{
	memset(ram, 0, sizeof(ram));
	memcpy(ram + 0x0801, image + 0x0801, sizeof(image) - 0x0801);
	memset(bank_ram, 0xA5, sizeof(bank_ram));
	memset(vram, 0, sizeof(vram));
	memcpy(bank_ram[3], text_code_image, sizeof(text_code_image));
	memcpy(bank_ram[4], commands_code_image, sizeof(commands_code_image));
	memcpy(bank_ram[5], logic_code_image, sizeof(logic_code_image));
	memcpy(bank_ram[6], irq_code_image, sizeof(irq_code_image));
	memcpy(bank_ram[7], parser_code_image, sizeof(parser_code_image));
	memcpy(bank_ram[0x10], memory_code_image, sizeof(memory_code_image));
	memcpy(bank_ram[0x0E], view_code_image, sizeof(view_code_image));
	memcpy(bank_ram[SOUND_BANK], sound_code_image, sizeof(sound_code_image));
	reset6502(false);
	regs.a = regs.x = regs.y = regs.db = regs.k = 0;
	regs.status = FLAG_CONSTANT;
	waiting = 0;
	clockticks6502 = 0;
	ram[0] = CALLER_BANK;
	ram[1] = 0x53;
	put_word(symbol("c_sp"), 0x9000);
	bank_window_reads = 0;
	out_of_window_reads = 0;
	track_source_reads = false;
	getin_count = getin_index = 0;
	getin_enabled = false;
	cbm_open_calls = 0;
	vera_address = 0;
	vera_stride = 0;
	vera_decrement = false;
	vera_data0_writes = 0;
	vera_data0_history_count = 0;
	capture_vera_data0_history = false;
}



static void call_target_caller(const char *name)
{
	regs.sp = 0x1FD;
	regs.a = regs.x = regs.y = 0;
	regs.status = FLAG_CONSTANT;
	ram[0x1FE] = 0xFF;
	ram[0x1FF] = 0x01;
	regs.pc = symbol(name);
	unsigned steps = 0;
	while (regs.pc != RETURN_PC)
	{
		if (++steps >= 1000000)
		{
			fprintf(stderr, "Production target caller timed out: PC=$%04X RAM=$%02X WAI=%u I=%u\n",
				regs.pc, ram[0], waiting, (regs.status & FLAG_INTERRUPT) != 0);
			CHECK(false, "Production target caller did not return");
		}
		step6502();
	}
	CHECK(regs.sp == 0x1FF, "Production text caller changed hardware stack");
	CHECK(get_word(symbol("c_sp")) == 0x9000, "Production text caller corrupted C argument stack");
	CHECK(ram[0] == CALLER_BANK && ram[1] == 0x53, "Production text caller did not restore caller banks");
}

#include "test_banked_strings_bounded_cases.h"

static void run_harness_smoke(void)
{
	prepare();
	call_target_caller("_testRunHarnessSmoke");
	CHECK(get_word(symbol("_testHarnessSmokeResult")) == 771,
		"Compiled-C test caller did not reach the production helpers-bank multiply");
	puts("Compiled-C per-bank test harness smoke passes");
}

int main(int argc, char **argv)
{
	CHECK((argc == 3 || argc == 4) && !strcmp(argv[1], "--build"),
		"Usage: test_banked_strings --build DIR [--smoke|--bounded]");
	load_image(argv[2]);
	if (argc == 4)
	{
		if (!strcmp(argv[3], "--smoke")) run_harness_smoke();
		else if (!strcmp(argv[3], "--bounded"))
		{
			run_bounded_cases();
			run_game_id_cases();
		}
		else CHECK(false, "Unknown production caller test case");
		return 0;
	}
	run_harness_smoke();
	run_bounded_cases();
	run_game_id_cases();
	return 0;
}
