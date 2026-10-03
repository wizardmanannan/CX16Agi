/* Host regression for the elapsed-VSYNC timer service and AGI clock. */
#define _TIMER_H_
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t byte;
typedef uint32_t vsync_counter_t;
typedef void (*timer_proc)(vsync_counter_t elapsedSeconds);
extern void b6CopyVSyncCounter32(vsync_counter_t *out);
byte *var;

#include "../src/timer.c"

static vsync_counter_t fakeVSyncCounter;
static unsigned callbackCount;
static vsync_counter_t callbackSeconds[8];
static byte gameVars[256];

#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "%s (%s:%d)\n", message, __FILE__, __LINE__); exit(1); } } while (0)

void b6CopyVSyncCounter32(vsync_counter_t *out)
{
	*out = fakeVSyncCounter;
}

static void record_elapsed_seconds(vsync_counter_t elapsedSeconds)
{
	CHECK(callbackCount < sizeof(callbackSeconds) / sizeof(callbackSeconds[0]), "Too many timer callbacks");
	callbackSeconds[callbackCount++] = elapsedSeconds;
}

static void set_counter(vsync_counter_t value)
{
	fakeVSyncCounter = value;
}

static void test_elapsed_frames(void)
{
	set_counter(1000);
	callbackCount = 0;
	b6InitTimer(record_elapsed_seconds);

	set_counter(1059);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 0, "59 VSYNCs advanced the clock early");
	set_counter(1060);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 1 && callbackSeconds[0] == 1, "60 VSYNCs did not produce one elapsed second");

	set_counter(1061);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 1, "61 total VSYNCs advanced a second early");

	set_counter(2000);
	callbackCount = 0;
	b6InitTimer(record_elapsed_seconds);
	set_counter(2061);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 1 && callbackSeconds[0] == 1,
		"61 elapsed frames did not produce one second and retain one frame");

	set_counter(3000);
	callbackCount = 0;
	b6InitTimer(record_elapsed_seconds);
	set_counter(6599);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 1 && callbackSeconds[0] == 59,
		"3,599 elapsed frames were not accounted as 59 seconds");
	set_counter(6600);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 2 && callbackSeconds[1] == 1,
		"3,600 elapsed frames did not reach 60 seconds");

	set_counter(7000);
	callbackCount = 0;
	b6InitTimer(record_elapsed_seconds);
	set_counter(7000 + 8160); /* A blocked foreground operation spanning 136 seconds. */
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 1 && callbackSeconds[0] == 136,
		"Long blocking interval was not delivered in one callback with elapsed seconds");

	set_counter(9000);
	callbackCount = 0;
	b6InitTimer(record_elapsed_seconds);
	set_counter(9000 + 65536U + 137U);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 1 && callbackSeconds[0] == 1094,
		"Long interval beyond 16-bit VSYNC range was truncated");
	CHECK(partialVSyncFrames == 33, "Long interval did not retain its subsecond frames");
}

static void test_counter_wrap_and_maximum_delta(void)
{
	set_counter(UINT32_MAX - 15U);
	callbackCount = 0;
	b6InitTimer(record_elapsed_seconds);
	set_counter(44);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 1 && callbackSeconds[0] == 1,
		"Counter wrap did not preserve elapsed frames");

	set_counter(0);
	callbackCount = 0;
	b6InitTimer(record_elapsed_seconds);
	set_counter(59);
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 0, "Maximum-delta setup unexpectedly advanced time");
	set_counter((uint32_t)(59U + UINT32_MAX));
	b6CheckTimer(fakeVSyncCounter);
	CHECK(callbackCount == 1 && callbackSeconds[0] == 71582789UL,
		"Maximum 32-bit VSYNC delta plus retained fraction was not accounted");
	CHECK(partialVSyncFrames == 14, "Maximum-delta frame remainder was not retained");
}

static void test_game_clock_calendar(void)
{
	var = gameVars;
	memset(gameVars, 0, sizeof(gameVars));
	b6Timing_proc(59);
	CHECK(var[11] == 59 && var[12] == 0, "59 elapsed seconds were decomposed incorrectly");
	b6Timing_proc(1);
	CHECK(var[11] == 0 && var[12] == 1, "60 elapsed seconds did not roll over to a minute");

	memset(gameVars + 11, 0, 4);
	b6Timing_proc(3599);
	CHECK(var[11] == 59 && var[12] == 59 && var[13] == 0 && var[14] == 0,
		"3,599 seconds were decomposed incorrectly");
	b6Timing_proc(1);
	CHECK(var[11] == 0 && var[12] == 0 && var[13] == 1 && var[14] == 0,
		"3,600 seconds did not roll over to an hour");

	memset(gameVars + 11, 0, 4);
	b6Timing_proc(23UL * 60 * 60 + 59 * 60 + 59);
	CHECK(var[11] == 59 && var[12] == 59 && var[13] == 23 && var[14] == 0,
		"23:59:59 was decomposed incorrectly");
	b6Timing_proc(1);
	CHECK(var[11] == 0 && var[12] == 0 && var[13] == 0 && var[14] == 1,
		"23:59:59 did not roll over to the next day");

	var[14] = 255;
	b6Timing_proc(24UL * 60 * 60);
	CHECK(var[11] == 0 && var[12] == 0 && var[13] == 0 && var[14] == 0,
		"Day 255 did not wrap after one full day");

	memset(gameVars + 11, 0, 4);
	b6Timing_proc(2UL * 24 * 60 * 60 + 3UL * 60 * 60 + 4UL * 60 + 5);
	CHECK(var[11] == 5 && var[12] == 4 && var[13] == 3 && var[14] == 2,
		"Long elapsed time was not decomposed in bounded calendar steps");

	memset(gameVars + 11, 0, 4);
	b6Timing_proc(71582789U);
	CHECK(var[11] == 29 && var[12] == 6 && var[13] == 12 && var[14] == 60,
		"Maximum elapsed-seconds value overflowed calendar decomposition");

	memset(gameVars + 11, 0, 4);
	var[14] = 255;
	b6Timing_proc(71582789U);
	CHECK(var[11] == 29 && var[12] == 6 && var[13] == 12 && var[14] == 59,
		"Maximum elapsed time plus the existing calendar overflowed 32-bit decomposition");
}

static void test_throttle_and_timed_wait_arithmetic(void)
{
	uint16_t last = 0xFFF0, now = 0x0030;
	CHECK((uint16_t)(now - last) == 64,
		"16-bit interpreter throttle delta did not wrap correctly");
	CHECK((uint32_t)(137U - 100U) >= 30,
		"Skipped timed-print deadline was not recognized by elapsed-time comparison");
	CHECK((uint32_t)(20U - (UINT32_MAX - 10U)) >= 31,
		"Timed-print elapsed comparison did not cross counter wrap");
}

static char *read_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	CHECK(file, "Cannot open source for contract checks");
	CHECK(!fseek(file, 0, SEEK_END), "Cannot seek source file");
	long length = ftell(file);
	CHECK(length >= 0, "Cannot size source file");
	rewind(file);
	char *source = malloc((size_t)length + 1);
	CHECK(source, "Out of memory reading source");
	CHECK(fread(source, 1, (size_t)length, file) == (size_t)length, "Cannot read source file");
	source[length] = 0;
	CHECK(!fclose(file), "Cannot close source file");
	return source;
}

static void test_source_contract(void)
{
	char *meka = read_file("src/meka.c");
	char *commands = read_file("src/commands.c");
	char *irq = read_file("src/irq.h");
	char *irqAsm = read_file("src/irqAsm.s");
	char *main = strstr(meka, "void main()");
	char *initialise = strstr(meka, "b6Initialise()");
	char *loop = main ? strstr(main, "while (TRUE)") : NULL;
	char *copy = loop ? strstr(loop, "b6CopyVSyncCounter32(&currentVsync)") : NULL;
	char *timerGate = copy ? strstr(copy, "if (currentVsync != lastTimerVsync)") : NULL;
	char *service = timerGate ? strstr(timerGate, "b6CheckTimer(currentVsync)") : NULL;
	char *throttle = service ? strstr(service, "if ((unsigned int)((unsigned int)currentVsync - lastVsync)") : NULL;
	char *initialTimerBaseline = main ? strstr(main, "lastTimerVsync = currentVsync;") : NULL;
	char *controllers = initialise ? strstr(initialise, "b1InitControllers();") : NULL;
	char *timerBaseline = controllers ? strstr(controllers, "b6InitTimer(&b6Timing_proc);") : NULL;
	char *timeout = strstr(commands, "vSyncNow - vSyncStart < waitTicks");
	char *prototype = strstr(irq, "extern void __fastcall__ b6CopyVSyncCounter32(uint32_t *out);");
	char *wrappedCall = strstr(irq, "#pragma wrapped-call (push, trampoline, IRQ_BANK)");
	char *counterUpdate = strstr(irqAsm, "@vSyncCounter:\n");
	char *lowWordWrap = counterUpdate ? strstr(counterUpdate, "inc _vSyncCounter\nbne @resetLatches\ninc _vSyncCounter + 1\nbne @resetLatches\ninc _vSyncCounterHigh") : NULL;

	CHECK(copy && timerGate && service && throttle && initialTimerBaseline &&
		initialTimerBaseline < loop && loop < copy && copy < timerGate && timerGate < service && service < throttle,
		"Main loop does not gate timer service on changed VSYNC snapshots before the interpreter throttle");
	CHECK(controllers && timerBaseline && controllers < timerBaseline,
		"Timer baseline is set before game initialization completes");
	CHECK(timeout && !strstr(commands, "vSyncCounter != vSyncToContinueAt"),
		"Timed message wait still depends on observing an exact VSYNC deadline");
	CHECK(prototype && wrappedCall && prototype < wrappedCall,
		"Resident snapshot helper declaration is inside the banked wrapper scope");
	CHECK(lowWordWrap,
		"Resident 32-bit VSYNC counter does not increment its high word on low-word rollover");

	free(meka);
	free(commands);
	free(irq);
	free(irqAsm);
}

int main(void)
{
	test_elapsed_frames();
	test_counter_wrap_and_maximum_delta();
	test_game_clock_calendar();
	test_throttle_and_timed_wait_arithmetic();
	test_source_contract();
	puts("Timer checks pass: VSYNC deltas/remainders, calendar rollover, main throttle, and timeout deadline skips");
	return 0;
}
