/* Execute the real cc65 caller and banked string helper in fake6502. */
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
#define OBJECT_CRYPTO_READ_GUARD 16u
#define SOUND_BOUNDS_MAX_COPY_CALLS 32u
#define MEMORY_MANAGEMENT_BANK 0x10u
#define SOUND_BANK 0x0Bu
#define GETIN_ADDRESS 0xFFE4u
#define CBM_SETLFS_ADDRESS 0xFFBAu
#define CBM_SETNAM_ADDRESS 0xFFBDu
#define CBM_OPEN_ADDRESS  0xFFC0u
#define CBM_CLOSE_ADDRESS 0xFFC3u
#define CBM_CHKIN_ADDRESS 0xFFC6u
#define CBM_CKOUT_ADDRESS  0xFFC9u
#define CBM_READST_ADDRESS 0xFFB7u
#define CBM_CLRCH_ADDRESS  0xFFCCu
#define CBM_BASIN_ADDRESS 0xFFCFu
#define CBM_BSOUT_ADDRESS  0xFFD2u /* CHROUT and BSOUT share the KERNAL output entry. */
#define VERA_ADDR_LOW 0x9F20u
#define VERA_ADDR_HIGH 0x9F21u
#define VERA_ADDR_BANK 0x9F22u
#define VERA_DATA0 0x9F23u
#define VERA_ISR 0x9F27u
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
static uint8_t object_data_code_image[BANK_SIZE];
static uint8_t object_predicate_code_image[BANK_SIZE];
static uint8_t show_obj_code_image[BANK_SIZE];
static uint8_t vram[0x20000];
static uint32_t vera_address;
static unsigned vera_stride;
static bool vera_decrement;
static bool service_text_render_irqs;
static bool monitor_null_sound;
static unsigned null_sound_reads;
static bool watch_text_buffer_overflow;
static uint16_t text_buffer_overflow_start;
static uint16_t text_buffer_overflow_end;
static unsigned text_buffer_overflow_writes;
static uint16_t first_text_buffer_overflow_address;
static unsigned vera_data0_writes;
static const unsigned vera_increments[16] = { 0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 40, 80, 160, 320, 640 };
static struct { uint32_t address; uint8_t value; } vera_data0_history[16];
static unsigned vera_data0_history_count;
static bool capture_vera_data0_history;
static struct { char name[100]; uint16_t address; } labels[8192];
static unsigned label_count;
static unsigned bank_window_reads;
static unsigned out_of_window_reads;
static bool track_source_reads;
static bool sound_bounds_monitor_requested;
static bool sound_bounds_periodic_active;
static bool sound_bounds_prefix_active;
static bool sound_bounds_prefix_seen;
static bool sound_bounds_copy_active;
static uint8_t sound_bounds_prefix_stack;
static uint8_t sound_bounds_periodic_stack;
static uint8_t sound_bounds_copy_stack;
static uint16_t sound_bounds_prefix_return;
static uint16_t sound_bounds_periodic_return;
static uint16_t sound_bounds_copy_return;
static uint16_t sound_bounds_prefix_entry;
static uint16_t sound_bounds_periodic_entry;
static uint16_t sound_bounds_copy_entry;
static uint16_t sound_bounds_resource_start;
static uint16_t sound_bounds_resource_end;
static uint16_t sound_bounds_allocation_end;
static uint16_t sound_bounds_channel_start[2];
static uint16_t sound_bounds_channel_end[2];
static uint8_t sound_bounds_source_bank;
static unsigned sound_bounds_copy_count;
static unsigned sound_bounds_active_copy_index;
static bool sound_bounds_copy_overflow;
static unsigned sound_bounds_copy_reads[SOUND_BOUNDS_MAX_COPY_CALLS];
static unsigned sound_bounds_outside_channel[SOUND_BOUNDS_MAX_COPY_CALLS];
static unsigned sound_bounds_outside_window[SOUND_BOUNDS_MAX_COPY_CALLS];
static unsigned sound_bounds_outside_resource[SOUND_BOUNDS_MAX_COPY_CALLS];
static unsigned sound_bounds_outside_allocation[SOUND_BOUNDS_MAX_COPY_CALLS];
static uint8_t getin_keys[64];
static unsigned getin_count;
static unsigned getin_index;
static bool getin_enabled;
static unsigned cbm_open_calls;
static bool allocator_oom_runtime_active;
static bool allocator_oom_output_capture_active;
static uint8_t allocator_oom_output[128];
static size_t allocator_oom_output_length;
static bool allocator_oom_output_overflow;
static bool object_file_io_enabled;
static const uint8_t *object_file_stream;
static unsigned object_file_stream_length;
static unsigned object_file_stream_position;
static uint8_t object_file_status;
static bool object_file_fail_first_read;
static bool object_file_read_failed;
static unsigned object_file_close_calls;
static bool object_buffer_write_monitor;
static uint16_t object_buffer_start;
static unsigned object_buffer_capacity;
static unsigned object_buffer_out_of_bounds_writes;
static bool object_buffer_read_monitor;
static unsigned object_buffer_read_length;
static unsigned object_buffer_prefix_reads;
static unsigned object_buffer_suffix_reads;
static bool object_crypto_read_active;
static uint8_t object_crypto_read_stack;
static uint16_t object_crypto_read_return;
static uint16_t object_crypto_read_entry;

#define CHECK(condition, message) do { \
	if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } \
} while (0)

static void advance_vera_address(void)
{
	if (vera_decrement)
		vera_address = (vera_address - vera_stride) & 0x1FFFFu;
	else
		vera_address = (vera_address + vera_stride) & 0x1FFFFu;
	/* VERA updates its address registers after each data-port access. */
	ram[VERA_ADDR_LOW] = (uint8_t)vera_address;
	ram[VERA_ADDR_HIGH] = (uint8_t)(vera_address >> 8);
	ram[VERA_ADDR_BANK] = (uint8_t)((ram[VERA_ADDR_BANK] & 0xFE) | (vera_address >> 16));
}

uint8_t read6502(uint16_t address, uint8_t bank)
{
	if (monitor_null_sound && address == 11) ++null_sound_reads;
	unsigned sound_copy_index = SOUND_BOUNDS_MAX_COPY_CALLS;
	(void)bank;
	if (object_crypto_read_active && ram[0] == 0x0D &&
		address >= BANK_START && address < BANK_END)
	{
		if (address >= object_buffer_start - OBJECT_CRYPTO_READ_GUARD &&
			address < object_buffer_start)
			++object_buffer_prefix_reads;
		if (address >= object_buffer_start + object_buffer_read_length &&
			address < object_buffer_start + object_buffer_read_length + OBJECT_CRYPTO_READ_GUARD)
			++object_buffer_suffix_reads;
	}
	if (sound_bounds_monitor_requested && sound_bounds_periodic_active &&
		sound_bounds_prefix_seen && !sound_bounds_prefix_active &&
		sound_bounds_copy_active && ram[0] == sound_bounds_source_bank &&
		!sound_bounds_copy_overflow)
	{
		if (address >= BANK_START && address < BANK_END &&
			sound_bounds_active_copy_index >= SOUND_BOUNDS_MAX_COPY_CALLS)
		{
			if (sound_bounds_copy_count < SOUND_BOUNDS_MAX_COPY_CALLS)
				sound_bounds_active_copy_index = sound_bounds_copy_count++;
			else
				sound_bounds_copy_overflow = true;
		}
		if (sound_bounds_active_copy_index < SOUND_BOUNDS_MAX_COPY_CALLS)
			sound_copy_index = sound_bounds_active_copy_index;
	}
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
	if (object_file_io_enabled && address == CBM_CHKIN_ADDRESS)
	{
		if (object_file_fail_first_read && !object_file_read_failed)
		{
			object_file_read_failed = true;
			regs.a = 5;
			regs.status |= FLAG_CARRY;
		}
		else
		{
			regs.a = 0;
			regs.status &= (uint8_t)~FLAG_CARRY;
		}
		return 0x60;
	}
	if (object_file_io_enabled && address == CBM_READST_ADDRESS)
	{
		regs.a = object_file_status;
		return 0x60;
	}
	if (object_file_io_enabled && address == CBM_BASIN_ADDRESS)
	{
		if (object_file_stream_position < object_file_stream_length)
		{
			regs.a = object_file_stream[object_file_stream_position++];
			object_file_status = object_file_stream_position == object_file_stream_length ? 0x40 : 0;
		}
		else
		{
			regs.a = 0;
			object_file_status = 0x40;
		}
		return 0x60;
	}
	if (object_file_io_enabled && address == CBM_CLRCH_ADDRESS)
	{
		return 0x60;
	}
	if (object_file_io_enabled && address == CBM_CLOSE_ADDRESS)
	{
		++object_file_close_calls;
		return 0x60;
	}
	if (address == CBM_OPEN_ADDRESS)
	{
		++cbm_open_calls;
		if (object_file_io_enabled)
		{
			object_file_stream_position = 0;
			object_file_status = object_file_stream_length == 0 ? 0x40 : 0;
			regs.a = 0;
			regs.status &= (uint8_t)~FLAG_CARRY;
		}
		else
		{
			regs.a = 1; /* Return a missing-file error so the real resolver stays empty. */
			regs.status |= FLAG_CARRY;
		}
		return 0x60;
	}
	if (allocator_oom_runtime_active && address == CBM_CKOUT_ADDRESS)
	{
		regs.status &= (uint8_t)~FLAG_CARRY;
		return 0x60;
	}
	if (allocator_oom_runtime_active && address == CBM_READST_ADDRESS)
	{
		regs.a = 0;
		return 0x60;
	}
	if (allocator_oom_runtime_active && address == CBM_CLRCH_ADDRESS)
		return 0x60;
	if (allocator_oom_runtime_active && address == CBM_BSOUT_ADDRESS)
	{
		if (allocator_oom_output_capture_active)
		{
			if (allocator_oom_output_length < sizeof(allocator_oom_output))
				allocator_oom_output[allocator_oom_output_length++] = regs.a;
			else
				allocator_oom_output_overflow = true;
		}
		return 0x60;
	}
	/* Missing-file diagnostics use the real CRT and the platform console. */
	if (address == CBM_CKOUT_ADDRESS || address == CBM_READST_ADDRESS ||
		address == CBM_CLRCH_ADDRESS || address == CBM_BSOUT_ADDRESS)
	{
		regs.a = 0;
		regs.status &= (uint8_t)~FLAG_CARRY;
		return 0x60;
	}
	if (address == VERA_DATA0)
	{
		uint8_t value = vram[vera_address & 0x1FFFFu];
		advance_vera_address();
		return value;
	}
	if (sound_copy_index < SOUND_BOUNDS_MAX_COPY_CALLS && address >= BANK_END)
	{
		++sound_bounds_outside_window[sound_copy_index];
		++sound_bounds_outside_resource[sound_copy_index];
		++sound_bounds_outside_allocation[sound_copy_index];
	}
	if (track_source_reads && address >= BANK_END) ++out_of_window_reads;
	if (address >= BANK_START && address < BANK_END)
	{
		if (sound_copy_index < SOUND_BOUNDS_MAX_COPY_CALLS)
		{
			unsigned channel;
			++sound_bounds_copy_reads[sound_copy_index];
			if (address < sound_bounds_resource_start || address >= sound_bounds_resource_end)
				++sound_bounds_outside_resource[sound_copy_index];
			if (address < sound_bounds_resource_start || address >= sound_bounds_allocation_end)
				++sound_bounds_outside_allocation[sound_copy_index];
			channel = sound_copy_index == 0 ? 0 : 1;
			if (address < sound_bounds_channel_start[channel] ||
				address >= sound_bounds_channel_end[channel])
				++sound_bounds_outside_channel[sound_copy_index];
		}
		if (track_source_reads) ++bank_window_reads;
		return bank_ram[ram[0]][address - BANK_START];
	}
	return ram[address];
}

void write6502(uint16_t address, uint8_t bank, uint8_t value)
{
	(void)bank;
	if (watch_text_buffer_overflow && ram[0] == 3 &&
		address >= text_buffer_overflow_start && address < text_buffer_overflow_end && value == 0x80)
	{
		if (!text_buffer_overflow_writes) first_text_buffer_overflow_address = address;
		++text_buffer_overflow_writes;
	}
	if (address == VERA_ISR && service_text_render_irqs)
	{
		/* The production IRQ clears VERA interrupt bits by writing one to them. */
		ram[address] &= (uint8_t)~value;
		return;
	}
	if (object_buffer_write_monitor && ram[0] == 0x0D &&
		address >= object_buffer_start + object_buffer_capacity && address < BANK_END)
		++object_buffer_out_of_bounds_writes;
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
	load_raw_bank_image(build, "agi.cx16.0D", object_data_code_image, "Cannot open object code bank image");
	load_raw_bank_image(build, "agi.cx16.0F", object_predicate_code_image,
		"Cannot open object predicate code bank image");
	load_raw_bank_image(build, "agi.cx16.11", show_obj_code_image,
		"Cannot open show.obj code bank image");
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

static void set_object_file_stream(const uint8_t *data, unsigned length, bool fail_first_read)
{
	object_file_io_enabled = true;
	object_file_stream = data;
	object_file_stream_length = length;
	object_file_stream_position = 0;
	object_file_status = length == 0 ? 0x40 : 0;
	object_file_fail_first_read = fail_first_read;
	object_file_read_failed = false;
	object_file_close_calls = 0;
}

static void monitor_object_buffer_writes(void)
{
	object_buffer_start = symbol("_bDObjData");
	object_buffer_capacity = get_word(symbol("_testObjectNameCacheSize"));
	CHECK(object_buffer_start >= BANK_START &&
		object_buffer_start + object_buffer_capacity <= BANK_END,
		"Production object input buffer is outside BANKRAM0D");
	object_buffer_out_of_bounds_writes = 0;
	object_buffer_write_monitor = true;
}

static void stop_monitoring_object_buffer_writes(void)
{
	object_buffer_write_monitor = false;
}

static void monitor_object_buffer_reads(unsigned fileLength)
{
	object_buffer_start = symbol("_bDObjData");
	object_buffer_capacity = get_word(symbol("_testObjectNameCacheSize"));
	CHECK(object_buffer_start >= BANK_START &&
		object_buffer_start + object_buffer_capacity <= BANK_END,
		"Production object input buffer is outside BANKRAM0D");
	CHECK(fileLength <= object_buffer_capacity,
		"Crypto read monitor fixture exceeds the production input buffer");
	object_buffer_read_length = fileLength;
	object_buffer_prefix_reads = 0;
	object_buffer_suffix_reads = 0;
	object_crypto_read_active = false;
	object_crypto_read_entry = symbol("_bDIsObjCrypt");
	object_buffer_read_monitor = true;
}

static void stop_monitoring_object_buffer_reads(void)
{
	object_buffer_read_monitor = false;
	object_crypto_read_active = false;
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
	memcpy(bank_ram[0x0D], object_data_code_image, sizeof(object_data_code_image));
	memcpy(bank_ram[0x0F], object_predicate_code_image, sizeof(object_predicate_code_image));
	memcpy(bank_ram[0x11], show_obj_code_image, sizeof(show_obj_code_image));
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
	sound_bounds_monitor_requested = false;
	sound_bounds_periodic_active = false;
	sound_bounds_copy_active = false;
	sound_bounds_copy_count = 0;
	sound_bounds_active_copy_index = SOUND_BOUNDS_MAX_COPY_CALLS;
	sound_bounds_copy_overflow = false;
	memset(sound_bounds_copy_reads, 0, sizeof(sound_bounds_copy_reads));
	memset(sound_bounds_outside_channel, 0, sizeof(sound_bounds_outside_channel));
	memset(sound_bounds_outside_window, 0, sizeof(sound_bounds_outside_window));
	memset(sound_bounds_outside_resource, 0, sizeof(sound_bounds_outside_resource));
	memset(sound_bounds_outside_allocation, 0, sizeof(sound_bounds_outside_allocation));
	getin_count = getin_index = 0;
	getin_enabled = false;
	cbm_open_calls = 0;
	allocator_oom_runtime_active = false;
	allocator_oom_output_capture_active = false;
	allocator_oom_output_length = 0;
	allocator_oom_output_overflow = false;
	object_file_io_enabled = false;
	object_file_stream = NULL;
	object_file_stream_length = 0;
	object_file_stream_position = 0;
	object_file_status = 0;
	object_file_fail_first_read = false;
	object_file_read_failed = false;
	object_file_close_calls = 0;
	object_buffer_write_monitor = false;
	object_buffer_start = 0;
	object_buffer_capacity = 0;
	object_buffer_out_of_bounds_writes = 0;
	object_buffer_read_monitor = false;
	object_buffer_read_length = 0;
	object_buffer_prefix_reads = 0;
	object_buffer_suffix_reads = 0;
	object_crypto_read_active = false;
	object_crypto_read_stack = 0;
	object_crypto_read_return = 0;
	object_crypto_read_entry = 0;
	vera_address = 0;
	vera_stride = 0;
	vera_decrement = false;
	vera_data0_writes = 0;
	vera_data0_history_count = 0;
	capture_vera_data0_history = false;
	service_text_render_irqs = false;
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
			fprintf(stderr, "Production target caller %s timed out: PC=$%04X RAM=$%02X SP=$%02X WAI=%u I=%u OBJECT open=%u stream=%u/%u status=$%02X close=%u readError=%u\n",
				name, regs.pc, ram[0], regs.sp, waiting,
				(regs.status & FLAG_INTERRUPT) != 0, cbm_open_calls,
				object_file_stream_position, object_file_stream_length,
				object_file_status, object_file_close_calls, object_file_read_failed);
			CHECK(false, "Production target caller did not return");
		}
		if (sound_bounds_monitor_requested)
		{
			if (!sound_bounds_periodic_active && ram[0] == SOUND_BANK &&
				regs.pc == sound_bounds_periodic_entry)
			{
				sound_bounds_periodic_active = true;
				sound_bounds_periodic_stack = (uint8_t)regs.sp;
				sound_bounds_periodic_return = (uint16_t)(
					ram[0x100 + (uint8_t)(regs.sp + 1)] |
					((uint16_t)ram[0x100 + (uint8_t)(regs.sp + 2)] << 8));
				++sound_bounds_periodic_return;
			}
			if (sound_bounds_periodic_active && !sound_bounds_prefix_seen &&
				!sound_bounds_prefix_active && regs.pc == sound_bounds_prefix_entry)
			{
				sound_bounds_prefix_active = true;
				sound_bounds_prefix_stack = (uint8_t)regs.sp;
				sound_bounds_prefix_return = (uint16_t)(
					ram[0x100 + (uint8_t)(regs.sp + 1)] |
					((uint16_t)ram[0x100 + (uint8_t)(regs.sp + 2)] << 8));
				++sound_bounds_prefix_return;
			}
			if (sound_bounds_periodic_active && sound_bounds_prefix_seen &&
				!sound_bounds_copy_active &&
				regs.pc == sound_bounds_copy_entry)
			{
				sound_bounds_copy_active = true;
				sound_bounds_copy_stack = (uint8_t)regs.sp;
				sound_bounds_copy_return = (uint16_t)(
					ram[0x100 + (uint8_t)(regs.sp + 1)] |
					((uint16_t)ram[0x100 + (uint8_t)(regs.sp + 2)] << 8));
				++sound_bounds_copy_return;
				sound_bounds_active_copy_index = SOUND_BOUNDS_MAX_COPY_CALLS;
			}
		}
		if (object_buffer_read_monitor && !object_crypto_read_active &&
			regs.pc == object_crypto_read_entry)
		{
			object_crypto_read_active = true;
			object_crypto_read_stack = (uint8_t)regs.sp;
			object_crypto_read_return = (uint16_t)(
				ram[0x100 + (uint8_t)(regs.sp + 1)] |
				((uint16_t)ram[0x100 + (uint8_t)(regs.sp + 2)] << 8));
			++object_crypto_read_return;
		}
		step6502();
		if (service_text_render_irqs)
		{
			/* Deliver VSYNC at WAI and let irq6502 run the production handler. */
			if (waiting) ram[VERA_ISR] |= 1;
			if (ram[VERA_ISR] & 1) irq6502();
		}
		if (object_crypto_read_active && regs.pc == object_crypto_read_return &&
			(uint8_t)regs.sp == (uint8_t)(object_crypto_read_stack + 2))
			object_crypto_read_active = false;
		if (sound_bounds_monitor_requested)
		{
			if (sound_bounds_prefix_active && regs.pc == sound_bounds_prefix_return &&
				(uint8_t)regs.sp == (uint8_t)(sound_bounds_prefix_stack + 2))
			{
				sound_bounds_prefix_active = false;
				sound_bounds_prefix_seen = true;
			}
			if (sound_bounds_copy_active && regs.pc == sound_bounds_copy_return &&
				(uint8_t)regs.sp == (uint8_t)(sound_bounds_copy_stack + 2))
			{
				sound_bounds_copy_active = false;
				sound_bounds_active_copy_index = SOUND_BOUNDS_MAX_COPY_CALLS;
			}
			if (sound_bounds_periodic_active && regs.pc == sound_bounds_periodic_return &&
				(uint8_t)regs.sp == (uint8_t)(sound_bounds_periodic_stack + 2))
				sound_bounds_periodic_active = false;
		}
	}
	CHECK(regs.sp == 0x1FF, "Production text caller changed hardware stack");
	CHECK(get_word(symbol("c_sp")) == 0x9000, "Production text caller corrupted C argument stack");
	CHECK(ram[0] == CALLER_BANK && ram[1] == 0x53, "Production text caller did not restore caller banks");
}

static bool call_target_caller_until_exit(const char *name)
{
	uint16_t exit_entry = symbol("_exit");
	unsigned steps = 0;

	regs.sp = 0x1FD;
	regs.a = regs.x = regs.y = 0;
	regs.status = FLAG_CONSTANT;
	ram[0x1FE] = 0xFF;
	ram[0x1FF] = 0x01;
	regs.pc = symbol(name);
	/* Stop before the CRT stores cc65's fastcall int argument from A/X in STATUS. */
	while (regs.pc != exit_entry && regs.pc != RETURN_PC)
	{
		if (++steps >= 1000000)
		{
			fprintf(stderr, "Production target caller %s did not reach _exit or return: PC=$%04X\n",
				name, regs.pc);
			CHECK(false, "Production target caller did not reach a process-exit boundary");
		}
		step6502();
	}
	return regs.pc == exit_entry;
}

#include "test_banked_strings_bounded_cases.h"
#include "test_banked_strings_input_cases.h"
#include "test_banked_strings_metadata_cases.h"
#include "test_banked_strings_allocator_cases.h"
#include "test_banked_strings_commands_cases.h"
#include "test_object_commands_cases.h"
#include "test_show_obj_allocation_cases.h"
#include "test_banked_strings_sound_cases.h"
#include "test_banked_strings_text_cases.h"
#include "test_timer_target_cases.h"

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
		"Usage: test_banked_strings --build DIR [--smoke|--bounded|--input|--metadata|--allocator-oom|--get-num|--object-commands|--object-crypto-bounds|--object-crypto-pointer|--show-object|--sound|--text-cleanup|--timer]");
	load_image(argv[2]);
	if (argc == 4)
	{
		if (!strcmp(argv[3], "--smoke")) run_harness_smoke();
		else if (!strcmp(argv[3], "--bounded"))
		{
			run_bounded_cases();
			run_game_id_cases();
		}
		else if (!strcmp(argv[3], "--input")) run_input_cases();
		else if (!strcmp(argv[3], "--metadata")) run_metadata_cases();
		else if (!strcmp(argv[3], "--allocator-oom")) run_allocator_oom_case();
		else if (!strcmp(argv[3], "--get-num")) run_commands_cases();
		else if (!strcmp(argv[3], "--object-commands")) run_object_commands_cases();
		else if (!strcmp(argv[3], "--object-crypto-bounds")) run_object_crypto_bounds_case();
		else if (!strcmp(argv[3], "--object-crypto-pointer")) run_object_crypto_pointer_case();
		else if (!strcmp(argv[3], "--show-object")) run_show_obj_allocation_cases();
		else if (!strcmp(argv[3], "--sound")) run_sound_cases();
		else if (!strcmp(argv[3], "--sound-cache")) run_sound_cache_capacity();
		else if (!strcmp(argv[3], "--sound-channels")) run_sound_valid_prefix_playback();
		else if (!strcmp(argv[3], "--sound-growth")) run_sound_output_growth();
		else if (!strcmp(argv[3], "--sound-discard")) run_repeated_sound_discard();
		else if (!strcmp(argv[3], "--text-cleanup")) run_text_cleanup_cases();
		else if (!strcmp(argv[3], "--timer")) run_timer_clock_cases();
		else CHECK(false, "Unknown production caller test case");
		return 0;
	}
	run_harness_smoke();
	run_bounded_cases();
	run_game_id_cases();
	run_input_cases();
	run_metadata_cases();
	run_allocator_oom_case();
	run_commands_cases();
	run_object_commands_cases();
	run_object_crypto_bounds_case();
	run_object_crypto_pointer_case();
	run_show_obj_allocation_cases();
	run_sound_cases();
	run_repeated_sound_discard();
	run_sound_cache_capacity();
	run_sound_output_growth();
	run_sound_valid_prefix_playback();
	run_text_cleanup_cases();
	run_timer_clock_cases();
	return 0;
}
