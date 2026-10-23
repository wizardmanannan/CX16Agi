#ifndef TEST_TIMER_TARGET_CASES_H
#define TEST_TIMER_TARGET_CASES_H

static void run_timer_clock_cases(void)
{
	static const struct {
		uint8_t before[4]; /* seconds, minutes, hours, days */
		uint32_t elapsed;
		uint8_t after[4];
	} cases[] = {
		{ {59, 59, 23, 0}, 1, {0, 0, 0, 1} },
		{ {59, 59, 23, 255}, 1, {0, 0, 0, 0} },
		{ {0, 0, 0, 1}, 0, {0, 0, 0, 1} },
		{ {7, 8, 9, 10}, 172800UL, {7, 8, 9, 12} }
	};
	unsigned i;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		uint16_t clock, elapsed;
		prepare();
		clock = symbol("_testTimerClock");
		elapsed = symbol("_testTimerElapsed");
		memcpy(ram + clock, cases[i].before, sizeof(cases[i].before));
		put_word(elapsed, (uint16_t)cases[i].elapsed);
		put_word(elapsed + 2, (uint16_t)(cases[i].elapsed >> 16));
		call_target_caller("_testRunTimerClock");
		if (memcmp(ram + clock, cases[i].after, sizeof(cases[i].after)))
			fprintf(stderr, "Compiled timer case %u: got %u:%02u:%02u day %u, expected %u:%02u:%02u day %u\n",
				i, ram[clock + 2], ram[clock + 1], ram[clock], ram[clock + 3],
				cases[i].after[2], cases[i].after[1], cases[i].after[0], cases[i].after[3]);
		CHECK(!memcmp(ram + clock, cases[i].after, sizeof(cases[i].after)),
			"Compiled production clock used the wrong seconds-per-day divisor");
	}
	puts("Compiled production timer passes day rollover, day-byte wrap and multi-day elapsed time");
}

#endif
