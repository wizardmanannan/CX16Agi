#ifndef TEST_BANKED_STRINGS_SOUND_CASES_H
#define TEST_BANKED_STRINGS_SOUND_CASES_H

static void run_sound_output_growth(void)
{
	prepare(); call_target_caller("_testSoundOutputGrowth");
	CHECK(ram[symbol("_testSoundGrowthCursorValid")],"Generated sound flush retained a freed allocation address or bank after growth");
	for(unsigned i=0;i<8;++i) CHECK(ram[symbol("_testSoundGrowthPrefix")+i]==0x52,"Sound growth damaged the channel prefix");
	for(unsigned i=0;i<220;++i) CHECK(ram[symbol("_testSoundGrowthBytes")+i]==(uint8_t)(i+1),"Sound growth lost generated output across a flush or relocation");
	puts("PASS generated sound output survives relocation, prefix accounting and multiple flushes");
}

static void run_sound_handler(void);

static void run_sound_valid_prefix_playback(void)
{
	for(unsigned count=0;count<=4;++count) {
		prepare();
		memset(bank_ram[0x13]+8,0xFF,8);
		ram[symbol("_testSoundPrefixChannels")]=(uint8_t)count;
		call_target_caller("_testPlaySoundValidPrefix");
		CHECK(bank_ram[SOUND_BANK][symbol("_bBChannelsPlaying")-BANK_START]==count,"Playback started a channel rejected by offset validation");
		for(unsigned i=count;i<4;++i) CHECK(!bank_ram[SOUND_BANK][symbol("_bBIsPlaying")-BANK_START+i],"Absent sound channel was marked playing");
		run_sound_handler();
		CHECK(!bank_ram[SOUND_BANK][symbol("_bBChannelsPlaying")-BANK_START],"Valid sound prefix did not finish after its real channel terminators");
		CHECK(ram[0x400+257+23],"Valid sound prefix did not signal its completion flag");
	}
	puts("PASS sound prefixes of zero through four channels play and complete without NULL reads");
}

static void run_sound_cache_capacity(void)
{
    static const uint8_t volume[]={0x12,0x34,0,16,0,8,0,10,0,12,0,14,0,255,255,255,255,255,255,255,255};
    uint16_t pointers[30];
    prepare(); call_target_caller("_testInitSoundResources");
    for(unsigned id=0;id<30;++id) {
        set_object_file_stream(volume,sizeof(volume),false);
        ram[symbol("_testSoundResourceId")]=(uint8_t)id;
        call_target_caller("_testLoadSoundResource");
        unsigned p=symbol("_bBLoadedSoundsPointer")-BANK_START+id*2;
        pointers[id]=bank_ram[SOUND_BANK][p]|((uint16_t)bank_ram[SOUND_BANK][p+1]<<8);
        CHECK(pointers[id],"Sound cache rejected a free slot");
        for(unsigned j=0;j<id;++j) CHECK(pointers[id]!=pointers[j],"Sound cache aliased live owners before capacity");
    }
    set_object_file_stream(volume,sizeof(volume),false);
    ram[symbol("_testSoundResourceId")]=30;
    call_target_caller("_testLoadSoundResource");
    unsigned p=symbol("_bBLoadedSoundsPointer")-BANK_START+60;
    CHECK(!bank_ram[SOUND_BANK][p]&&!bank_ram[SOUND_BANK][p+1],"Full sound cache overwrote a live record");
    ram[symbol("_testSoundResourceId")]=10;
    call_target_caller("_testDiscardSoundResource");
    ram[symbol("_testSoundResourceId")]=30;
    set_object_file_stream(volume,sizeof(volume),false);
    call_target_caller("_testLoadSoundResource");
    CHECK((bank_ram[SOUND_BANK][p]|((uint16_t)bank_ram[SOUND_BANK][p+1]<<8))==pointers[10],"Sound cache did not reuse its freed middle slot");
    puts("PASS sound cache preserves 30 owners, rejects overflow and reuses a middle slot");
}

static void run_repeated_sound_discard(void)
{
	prepare();
	call_target_caller("_testPrepareRepeatedSoundDiscard");
	CHECK(bank_ram[0x10][symbol("_allocationArray")-BANK_START+1],"Discard neighbour was not allocated");
	/* A NULL SoundFile aliases unrelated machine state, not an empty record. */
	ram[8]=ram[symbol("_testSoundDiscardNeighbourBank")];
	put_word(9,get_word(symbol("_testSoundDiscardNeighbour")));
	ram[11]=0;
	null_sound_reads=0; monitor_null_sound=true;
	call_target_caller("_testRepeatSoundDiscard");
	monitor_null_sound=false;
	CHECK(!null_sound_reads,"Repeated sound discard dereferenced a NULL SoundFile in zero page");
	CHECK(bank_ram[0x10][symbol("_allocationArray")-BANK_START+1],"Repeated sound discard freed an unrelated allocation through NULL");
	puts("PASS repeated sound discard preserves unrelated allocator owners");
}

static void run_sound_handler(void)
{
	uint8_t previousBank = ram[0];
	unsigned steps = 0;

	regs.sp = 0x1FD;
	regs.a = regs.x = regs.y = 0;
	regs.status = FLAG_CONSTANT;
	ram[0x1FE] = 0xFF;
	ram[0x1FF] = 0x01;
	ram[0] = SOUND_BANK;
	regs.pc = symbol("bBSoundHandler");
	while (regs.pc != RETURN_PC)
	{
		CHECK(++steps < 100000, "Production PSG sound handler did not return");
		step6502();
	}
	CHECK(regs.sp == 0x1FF, "Production PSG sound handler changed hardware stack");
	CHECK(ram[0] == SOUND_BANK, "Production PSG sound handler left its code bank selected incorrectly");
	ram[0] = previousBank;
}

static void run_sound_conversion(void)
{
	static const uint8_t expectedRaw12[12] = { 2, 0, 44, 4, 15, 8, 0, 200, 0, 0x8F, 0xFF, 0xFF };
	static const uint8_t expectedRaw0[12] = { 2, 0, 44, 4, 63, 8, 0, 200, 0, 0xBF, 0xFF, 0xFF };
	static const uint32_t noisePsgAddresses[5] = { 0x1F9CE, 0x1F9CC, 0x1F9CD, 0x1F9CE, 0x1F9CF };
	static const uint8_t expectedPsgRaw12[5] = { 0, 200, 0, 0xCF, 0x3F };
	static const uint8_t expectedPsgRaw0[5] = { 0, 200, 0, 0xFF, 0x3F };
	uint16_t outputBytes = symbol("_testSoundConversionNoiseBytes");
	uint16_t outputBanks = symbol("_testSoundConversionBanks");
	uint16_t sourceBanks = symbol("_testSoundConversionSourceBanks");
	uint16_t resources = symbol("_testSoundConversionResources");
	uint16_t noiseAddresses = symbol("_testSoundConversionNoiseAddresses");
	uint16_t sourcesReleased = symbol("_testSoundConversionSourcesReleased");
	uint16_t outputsReleased = symbol("_testSoundConversionOutputsReleased");
	uint16_t resource0, resource1, noise0, noise1;
	unsigned i;

	prepare();
	call_target_caller("_testRunSoundConversion");
	CHECK(read6502((uint16_t)(outputBanks), 0) == 0x13 &&
		read6502((uint16_t)(outputBanks + 1), 0) == 0x13,
		"Sound conversion did not use real allocator banks for both generated resources");
	CHECK(read6502(sourceBanks, 0) == 0x13 && read6502((uint16_t)(sourceBanks + 1), 0) == 0x13,
		"Sound conversion source allocations did not come from the first tiny allocator bank");
	CHECK(read6502(sourcesReleased, 0) && read6502((uint16_t)(sourcesReleased + 1), 0),
		"Periodic conversion retained its source resource allocation");
	resource0 = get_word(resources);
	resource1 = get_word((uint16_t)(resources + 2));
	noise0 = get_word(noiseAddresses);
	noise1 = get_word((uint16_t)(noiseAddresses + 2));
	CHECK(resource0 >= BANK_START && resource0 < BANK_END &&
		resource1 >= BANK_START && resource1 < BANK_END && resource0 != resource1,
		"Periodic conversion did not publish distinct owned output allocations");
	CHECK(noise0 == (uint16_t)(resource0 + 24) && noise1 == (uint16_t)(resource1 + 24),
		"Periodic conversion published an incorrect noise-channel pointer");

	CHECK(read6502((uint16_t)(outputBytes + 5), 0) == 8 &&
		read6502((uint16_t)(outputBytes + 6), 0) == 0,
		"Periodic noise generated an incorrect note duration");
	CHECK(read6502((uint16_t)(outputBytes + 7), 0) == 200 &&
		read6502((uint16_t)(outputBytes + 8), 0) == 0,
		"White-noise duration did not advance the periodic note to channel-2 frequency 300 (noise frequency 200)");
	CHECK(read6502((uint16_t)(outputBytes + 9), 0) == 0x8F,
		"Already converted SN76489 volume 12 was not preserved as CX16 volume 15");
	for (i = 0; i < sizeof(expectedRaw12); ++i)
		CHECK(read6502((uint16_t)(outputBytes + i), 0) == expectedRaw12[i],
			"Raw-volume-12 periodic noise output did not match all generated bytes");
	for (i = 0; i < sizeof(expectedRaw0); ++i)
		CHECK(read6502((uint16_t)(outputBytes + 12 + i), 0) == expectedRaw0[i],
			"Raw-volume-0 periodic noise output did not match all generated bytes");

	call_target_caller("_testRunSoundPlayRaw12");
	for (i = 0; i < 3; ++i) run_sound_handler();
	vera_data0_history_count = 0;
	capture_vera_data0_history = true;
	run_sound_handler();
	capture_vera_data0_history = false;
	CHECK(vera_data0_history_count == 5,
		"Production sound handler did not publish the periodic noise registers on its fourth tick");
	for (i = 0; i < 5; ++i)
	{
		CHECK(vera_data0_history[i].address == noisePsgAddresses[i],
			"Production sound handler wrote outside the periodic-noise PSG registers");
		CHECK(vera_data0_history[i].value == expectedPsgRaw12[i],
			"Production sound handler played the wrong frequency, volume or waveform for raw volume 12");
	}

	call_target_caller("_testRunSoundPlayRaw0");
	for (i = 0; i < 3; ++i) run_sound_handler();
	vera_data0_history_count = 0;
	capture_vera_data0_history = true;
	run_sound_handler();
	capture_vera_data0_history = false;
	CHECK(vera_data0_history_count == 5,
		"Production sound handler did not publish the periodic noise registers for raw volume 0");
	for (i = 0; i < 5; ++i)
	{
		CHECK(vera_data0_history[i].address == noisePsgAddresses[i],
			"Production sound handler wrote outside the periodic-noise PSG registers");
		CHECK(vera_data0_history[i].value == expectedPsgRaw0[i],
			"Production sound handler lost six-bit volume, frequency or waveform for raw volume 0");
	}

	call_target_caller("_testCleanupSoundConversion");
	CHECK(read6502(outputsReleased, 0) && read6502((uint16_t)(outputsReleased + 1), 0),
		"Sound conversion test did not release its generated allocations");
	puts("Production periodic-noise conversion passes byte generation, allocator ownership and PSG playback");
}

static bool sound_bounds_case_failed;

static void sound_bounds_expect(bool condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "%s\n", message);
		sound_bounds_case_failed = true;
	}
}

static void begin_sound_bounds_monitor(void)
{
	sound_bounds_resource_start = get_word(symbol("_testSoundBoundsResource"));
	sound_bounds_resource_end = get_word(symbol("_testSoundBoundsResourceEnd"));
	sound_bounds_allocation_end = get_word(symbol("_testSoundBoundsAllocationEnd"));
	sound_bounds_channel_start[0] = get_word(symbol("_testSoundBoundsChannel2Start"));
	sound_bounds_channel_end[0] = get_word(symbol("_testSoundBoundsChannel2End"));
	sound_bounds_channel_start[1] = get_word(symbol("_testSoundBoundsNoiseStart"));
	sound_bounds_channel_end[1] = get_word(symbol("_testSoundBoundsNoiseEnd"));
	sound_bounds_source_bank = ram[symbol("_testSoundBoundsBank")];
	sound_bounds_prefix_entry = symbol("_memCpyBankedBetween");
	sound_bounds_periodic_entry = symbol("_bBPreComputePeriodicSound");
	sound_bounds_copy_entry = symbol("_memCpyBanked");
	sound_bounds_periodic_active = false;
	sound_bounds_prefix_active = false;
	sound_bounds_prefix_seen = false;
	sound_bounds_copy_active = false;
	sound_bounds_copy_count = 0;
	sound_bounds_active_copy_index = SOUND_BOUNDS_MAX_COPY_CALLS;
	sound_bounds_copy_overflow = false;
	memset(sound_bounds_copy_reads, 0, sizeof(sound_bounds_copy_reads));
	memset(sound_bounds_outside_channel, 0, sizeof(sound_bounds_outside_channel));
	memset(sound_bounds_outside_window, 0, sizeof(sound_bounds_outside_window));
	memset(sound_bounds_outside_resource, 0, sizeof(sound_bounds_outside_resource));
	memset(sound_bounds_outside_allocation, 0, sizeof(sound_bounds_outside_allocation));
	sound_bounds_monitor_requested = true;
}

static void end_sound_bounds_monitor(void)
{
	sound_bounds_monitor_requested = false;
	sound_bounds_expect(sound_bounds_prefix_seen,
		"Periodic sound conversion did not finish its real source-prefix copy");
	sound_bounds_expect(!sound_bounds_periodic_active && !sound_bounds_prefix_active && !sound_bounds_copy_active,
		"Sound source-read monitor did not stop at the production helper return");
	sound_bounds_expect(!sound_bounds_copy_overflow,
		"Sound source-read monitor exceeded its per-copy counter capacity");
}

static void run_sound_bounds_short_resource(void)
{
	prepare();
	call_target_caller("_testPrepareSoundBounds");
	begin_sound_bounds_monitor();
	call_target_caller("_testRunSoundBounds");
	end_sound_bounds_monitor();
	sound_bounds_expect(sound_bounds_copy_count >= 2,
		"Periodic sound conversion did not perform both channel source copies");
	if (sound_bounds_copy_count >= 2)
	{
		sound_bounds_expect(sound_bounds_copy_reads[0] > 0 && sound_bounds_copy_reads[1] > 0,
			"Periodic sound source monitor did not observe actual CPU reads");
		sound_bounds_expect(sound_bounds_outside_channel[0] == 0 &&
			sound_bounds_outside_channel[1] == 0,
			"Periodic sound refresh read outside a channel's actual source span");
		sound_bounds_expect(sound_bounds_outside_resource[0] == 0 &&
			sound_bounds_outside_resource[1] == 0,
			"Periodic sound refresh read beyond the loaded sound resource");
		sound_bounds_expect(sound_bounds_outside_window[0] == 0 &&
			sound_bounds_outside_window[1] == 0,
			"Periodic sound refresh read past the banked RAM window");
		sound_bounds_expect(sound_bounds_outside_allocation[0] == 0 &&
			sound_bounds_outside_allocation[1] == 0,
			"Periodic sound refresh read beyond the allocator-owned source block");
	}
	call_target_caller("_testCleanupSoundBounds");
	sound_bounds_expect(ram[symbol("_testSoundBoundsOutputReleased")] != 0,
		"Short sound source-bound case did not release its generated output allocation");
}

static void run_sound_refill_long_resource(void)
{
	static const unsigned expectedCopyReads[] = { 12, 166, 166, 166, 166, 88 };
	static const uint8_t expectedPeriodicRecord[5] = { 8, 0, 200, 0, 0x8F };
	static const uint8_t expectedTerminator[2] = { 0xFF, 0xFF };
	const unsigned whiteNoteCount = 149;
	const unsigned expectedOutputBytes = whiteNoteCount * 5 + sizeof(expectedPeriodicRecord) + sizeof(expectedTerminator);
	uint16_t outputNoise;
	uint16_t outputResource;
	uint8_t outputBank;
	unsigned mismatchOffset = 0;
	uint8_t expectedByte = 0, actualByte = 0;
	unsigned i;
	bool outputMatches = true;
	bool outputPointerValid;

	prepare();
	call_target_caller("_testPrepareSoundRefill");
	begin_sound_bounds_monitor();
	call_target_caller("_testRunSoundBounds");
	end_sound_bounds_monitor();
	sound_bounds_expect(sound_bounds_copy_count == sizeof(expectedCopyReads) / sizeof(expectedCopyReads[0]),
		"Long periodic-noise refill did not use the expected channel and bounded-refill copies");
	for (i = 0; i < sound_bounds_copy_count && i < SOUND_BOUNDS_MAX_COPY_CALLS; ++i)
	{
		if (i < sizeof(expectedCopyReads) / sizeof(expectedCopyReads[0]))
			sound_bounds_expect(sound_bounds_copy_reads[i] == expectedCopyReads[i],
				"Long periodic-noise refill did not copy the exact channel/refill span");
		sound_bounds_expect(sound_bounds_outside_channel[i] == 0,
			"Long periodic-noise refill read outside its channel's source span");
		sound_bounds_expect(sound_bounds_outside_resource[i] == 0,
			"Long periodic-noise refill read beyond the loaded sound resource");
		sound_bounds_expect(sound_bounds_outside_allocation[i] == 0,
			"Long periodic-noise refill read beyond the allocator-owned source block");
		sound_bounds_expect(sound_bounds_outside_window[i] == 0,
			"Long periodic-noise refill read at or beyond $C000");
	}

	outputNoise = get_word(symbol("_testSoundBoundsOutputNoise"));
	outputResource = get_word(symbol("_testSoundBoundsOutputResource"));
	outputBank = ram[symbol("_testSoundBoundsOutputBank")];
	outputPointerValid = outputNoise >= BANK_START &&
		(unsigned)outputNoise + expectedOutputBytes <= BANK_END;
	sound_bounds_expect(outputPointerValid,
		"Long periodic-noise output stream did not fit inside the banked RAM window");
	sound_bounds_expect(outputNoise == (uint16_t)(outputResource + 24),
		"Long periodic-noise conversion published an unexpected output channel offset");
	if (outputPointerValid)
	{
		const uint8_t *output = bank_ram[outputBank] + outputNoise - BANK_START;
		for (i = 0; i < whiteNoteCount && outputMatches; ++i)
		{
			uint16_t duration = i == 50 ? 256 : 1;
			uint8_t expectedNote[5] = {
				(uint8_t)duration, (uint8_t)(duration >> 8), 44, 4, 15
			};
			unsigned j;
			for (j = 0; j < sizeof(expectedNote); ++j)
			{
				unsigned offset = i * sizeof(expectedNote) + j;
				if (output[offset] != expectedNote[j])
				{
					outputMatches = false;
					mismatchOffset = offset;
					expectedByte = expectedNote[j];
					actualByte = output[offset];
					break;
				}
			}
		}
		for (i = 0; i < sizeof(expectedPeriodicRecord) && outputMatches; ++i)
		{
			unsigned offset = whiteNoteCount * 5 + i;
			if (output[offset] != expectedPeriodicRecord[i])
			{
				outputMatches = false;
				mismatchOffset = offset;
				expectedByte = expectedPeriodicRecord[i];
				actualByte = output[offset];
			}
		}
		for (i = 0; i < sizeof(expectedTerminator) && outputMatches; ++i)
		{
			unsigned offset = whiteNoteCount * 5 + sizeof(expectedPeriodicRecord) + i;
			if (output[offset] != expectedTerminator[i])
			{
				outputMatches = false;
				mismatchOffset = offset;
				expectedByte = expectedTerminator[i];
				actualByte = output[offset];
			}
		}
	}
	else
		outputMatches = false;
	if (!outputMatches)
		fprintf(stderr, "Long periodic-noise output mismatch at byte %u: expected %02X, got %02X (expected length %u)\n",
			mismatchOffset, (unsigned)expectedByte, (unsigned)actualByte, expectedOutputBytes);
	sound_bounds_expect(outputMatches,
		"Long periodic-noise output did not contain every converted note, final periodic record and terminator");
	call_target_caller("_testCleanupSoundBounds");
	sound_bounds_expect(ram[symbol("_testSoundBoundsOutputReleased")] != 0,
		"Long sound refill case did not release its generated output allocation");
}

static void run_sound_offset_prefix_case(void)
{
	uint16_t pointers;

	prepare();
	call_target_caller("_testSoundSetOffsetPrefix");
	pointers = symbol("_testSoundOffsetPrefixPointers");
	CHECK(ram[symbol("_testSoundOffsetPrefixCount")] == 3,
		"A valid sound channel was rejected because the following offset was invalid");
	CHECK(get_word(pointers) == 0xA008 && get_word((uint16_t)(pointers + 2)) == 0xA00A &&
		get_word((uint16_t)(pointers + 4)) == 0xA00C &&
		get_word((uint16_t)(pointers + 6)) == 0,
		"Sound channel setup did not publish the valid prefix and clear absent channels");
}

static void run_sound_cases(void)
{
	sound_bounds_case_failed = false;
	run_sound_conversion();
	run_sound_bounds_short_resource();
	run_sound_refill_long_resource();
	run_sound_offset_prefix_case();
	CHECK(!sound_bounds_case_failed,
		"Production sound source bounds or long-channel refill regression detected");
}

#endif
