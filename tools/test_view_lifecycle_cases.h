static const uint8_t *view_resource_volume(unsigned loops, unsigned transparent, unsigned *length)
{
	static uint8_t volume[128];
	unsigned loop=5+loops*2, payload=loop+8;
	memset(volume,0,sizeof(volume));
	volume[0]=0x12; volume[1]=0x34; volume[3]=(uint8_t)payload;
	volume[7]=(uint8_t)loops;
	for(unsigned i=0;i<loops;++i) volume[10+i*2]=(uint8_t)loop;
	volume[5+loop]=1; volume[6+loop]=3;
	volume[8+loop]=1; volume[9+loop]=1; volume[10+loop]=(uint8_t)transparent;
	volume[11+loop]=0x11;
	*length=payload+5; return volume;
}

static void prepare_view_resource(unsigned loops, unsigned transparent)
{
	unsigned length;
	const uint8_t *volume=view_resource_volume(loops,transparent,&length);
	prepare_text_cleanup_case();
	set_object_file_stream(volume,length,false);
	call_target_caller("_testInitViewResource");
	call_target_caller("_testLoadViewLoops");
}

static void run_view_header_cases(void)
{
	static const unsigned counts[]={7,8,14,15};
	for(unsigned i=0;i<sizeof(counts)/sizeof(*counts);++i) {
		prepare_view_resource(counts[i],0);
		CHECK(ram[symbol("_testViewLoaded")],"Valid multi-loop view was rejected");
		CHECK(!ram[symbol("_testViewLoopErrors")],"View loader used stale offsets beyond its seven-loop initial header");
	}
	puts("PASS production view loader decodes every loop at 7/8/14/15 boundaries");
}

static void run_cached_closeup_case(void)
{
	prepare_view_resource(1,14);
	CHECK(ram[symbol("_testViewLoaded")],"Close-up fixture did not load its real view resource");
	call_target_caller("_testResetCachedCloseup");
	getin_enabled=true; getin_keys[0]=getin_keys[1]=13; getin_count=2;
	service_text_render_irqs=true;
	for(unsigned i=0;i<2;++i) {
		call_target_caller("_testShowCachedCloseup");
		CHECK(vram[0x1ffe7]==4,"Cached show.obj published an invalidated palette and wrong sprite dimensions after room reset");
	}
	service_text_render_irqs=false;
	CHECK(getin_index==2,"Close-up did not complete both real input/cleanup cycles");
	puts("PASS cached show.obj repairs its palette after room reset and survives repeated display/cleanup");
}
