/* Native W65C02 checks for completion waits and sliced-clear video ownership. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "registers.h"
#include "fake6502.h"
#include "test_paths.h"

extern struct regs regs;
enum {
    PRIORITY_START = (320 * 240) / 2,
    PRIORITY_SIZE = (160 * 168) / 2,
    PRIORITY_SLICE_SIZE = 2048,
    PRIORITY_TAIL_START = PRIORITY_START + (PRIORITY_SIZE / PRIORITY_SLICE_SIZE) * PRIORITY_SLICE_SIZE,
    PRIORITY_TAIL_SIZE = PRIORITY_SIZE % PRIORITY_SLICE_SIZE
};
static uint8_t ram[65536], vram[131072], vera_ctrl, dc_video, dc_hstart, fx_ctrl, fx_cache[4];
static unsigned addr[2], stride[2];
static unsigned data_writes;
static struct { char name[100]; unsigned address; } symbols[16];
static unsigned symbol_count;

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "%s (%s:%d)\n", m, __FILE__, __LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    if (address == 0x9F25) return vera_ctrl;
    if (address >= 0x9F20 && address <= 0x9F22) {
        unsigned port = vera_ctrl & 1;
        if (address == 0x9F20) return (uint8_t)addr[port];
        if (address == 0x9F21) return (uint8_t)(addr[port] >> 8);
        return (uint8_t)((addr[port] >> 16) | (stride[port] << 4));
    }
    if (address == 0x9F23 || address == 0x9F24) {
        unsigned port = address - 0x9F23;
        uint8_t value = vram[addr[port]];
        addr[port] = (addr[port] + stride[port]) & 0x1FFFF;
        return value;
    }
    if (address >= 0x9F29 && address <= 0x9F2C) {
        unsigned dcsel = (vera_ctrl >> 1) & 0x3F;
        unsigned reg = address - 0x9F29;
        if (dcsel == 0 && reg == 0) return dc_video;
        if (dcsel == 1 && reg == 0) return dc_hstart;
        if (dcsel == 2 && reg == 0) return fx_ctrl;
        if (dcsel == 6) return fx_cache[reg];
    }
    return ram[address];
}

void write6502(uint16_t address, uint8_t bank, uint8_t value) {
    (void)bank;
    if (address == 0x9F25) { vera_ctrl = value; return; }
    if (address >= 0x9F20 && address <= 0x9F22) {
        static const unsigned increments[16] = {0,1,2,4,8,16,32,64,128,256,512,40,80,160,320,640};
        unsigned port = vera_ctrl & 1;
        ram[address] = value;
        if (address == 0x9F20) addr[port] = (addr[port] & 0x1FF00) | value;
        else if (address == 0x9F21) addr[port] = (addr[port] & 0x100FF) | (value << 8);
        else {
            addr[port] = (addr[port] & 0xFFFF) | ((value & 1) << 16);
            stride[port] = increments[value >> 4];
        }
        return;
    }
    if (address >= 0x9F29 && address <= 0x9F2C) {
        unsigned dcsel = (vera_ctrl >> 1) & 0x3F;
        unsigned reg = address - 0x9F29;
        if (dcsel == 0 && reg == 0) dc_video = value;
        else if (dcsel == 1 && reg == 0) dc_hstart = value;
        else if (dcsel == 2 && reg == 0) fx_ctrl = value;
        else if (dcsel == 6) fx_cache[reg] = value;
        else ram[address] = value;
        return;
    }
    if (address == 0x9F23 || address == 0x9F24) {
        unsigned port = address - 0x9F23;
        if (fx_ctrl & 0x40) {
            CHECK(value == 0, "Priority FX flush must write the zero mask");
            CHECK(((vera_ctrl >> 1) & 0x3F) == 2, "Priority FX flush must use DCSEL2");
            unsigned base = addr[port] & ~3U;
            for (unsigned i = 0; i < 4; ++i) { vram[base + i] = fx_cache[i]; ++data_writes; }
        } else {
            vram[addr[port]] = value;
            ++data_writes;
        }
        addr[port] = (addr[port] + stride[port]) & 0x1FFFF;
        return;
    }
    ram[address] = value;
}

uint8_t memory_get_ram_bank(void) { return ram[0]; }
uint8_t memory_get_rom_bank(void) { return ram[1]; }
void vp6502(void) {}
void stop6502(uint16_t address, uint8_t bank) { (void)address; (void)bank; CHECK(false, "IRQ fixture stopped the CPU"); }

static char *read_file(const char *path) {
    FILE *file=fopen(path,"rb"); CHECK(file,"Cannot read IRQ assembly");
    CHECK(!fseek(file,0,SEEK_END),"Cannot seek IRQ assembly"); long n=ftell(file); CHECK(n>=0,"Cannot size IRQ assembly"); rewind(file);
    char *data=malloc((size_t)n+1); CHECK(data,"Out of memory"); CHECK(fread(data,1,(size_t)n,file)==(size_t)n,"Cannot read IRQ assembly");
    data[n]=0; fclose(file); return data;
}
static void write_file(const char *path,const char *data) {
    FILE *file=fopen(path,"wb"); CHECK(file,"Cannot create fixture"); CHECK(fwrite(data,1,strlen(data),file)==strlen(data),"Cannot write fixture"); fclose(file);
}
static void run_tool(const char *name,char *const args[]) {
    char tool[4096]; const char *home=getenv("CC65_HOME");
    if(home) snprintf(tool,sizeof(tool),"%s/bin/%s",home,name); else strcpy(tool,name);
    pid_t pid=fork(); CHECK(pid>=0,"Cannot fork tool");
    if(!pid) { char *argv[12]; unsigned n=0; while(args[n]) { CHECK(n<11,"Too many tool args"); argv[n]=args[n]; ++n; } argv[0]=tool; argv[n]=NULL; execv(tool,argv); perror(tool); _exit(127); }
    int status; CHECK(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&!WEXITSTATUS(status),"Assembler or linker failed");
}
static unsigned symbol(const char *name) {
    for(unsigned i=0;i<symbol_count;++i) if(!strcmp(name,symbols[i].name)) return symbols[i].address;
    fprintf(stderr,"Missing assembler symbol: %s\n",name); exit(1);
}

static void assemble(void) {
    char source[8192],config[8192],object[8192],binary[8192],labels[8192],fixture[12000];
    suite_test_path(source,sizeof(source),suite_fixture_directory,"irq_completion.s"); suite_test_path(config,sizeof(config),suite_fixture_directory,"irq_completion.cfg");
    suite_test_path(object,sizeof(object),suite_output_directory,"irq_completion.o"); suite_test_path(binary,sizeof(binary),suite_output_directory,"irq_completion.bin");
    suite_test_path(labels,sizeof(labels),suite_output_directory,"irq_completion.lbl");
    char *irq=read_file("src/irqAsm.s");
    char *graphics=read_file("src/graphicsAsm.s");
    char *priority=strstr(graphics,"_b8ClearPrioritySliceReset:"); CHECK(priority,"Priority clear slice missing");
    char *priority_end=strstr(priority,"\n_b8ClearBackground:"); CHECK(priority_end,"Priority clear slice end missing");
    char *wait=strstr(irq,".macro WAIT_FOR_IRQ_COMPLETION"); CHECK(wait,"Completion wait macro missing");
    char *wait_end=strstr(wait,"\n.endmacro"); CHECK(wait_end,"Completion wait macro end missing"); wait_end+=strlen("\n.endmacro");
    char *clear=strstr(irq,"@clear:"); CHECK(clear,"Clear start missing");
    char *clear_end=strstr(clear,"\n@showObj:"); CHECK(clear_end,"Clear start end missing");
    char *service=strstr(irq,"@serviceClear:"); CHECK(service,"Clear service missing");
    char *service_end=strstr(service,"\n@serviceClearMap:"); CHECK(service_end,"Clear service end missing");
    int size=snprintf(fixture,sizeof(fixture),
        ".include \"x16.inc\"\n.segment \"CODE\"\n"
        "IRQ_WORK_CLEAR = 1\nIRQ_CLEAR_STAGE_MAP = 0\nIRQ_CLEAR_STAGE_BACKGROUND = 1\nPICTURE_CODE_OVERFLOW_BANK = 14\n"
        "PRIORITY_START = (320*240)/2\nPRIORITY_SIZE = (160*168)/2\nPRIORITY_SLICE_SIZE = 2048\nPRIORITY_TAIL_SIZE = (PRIORITY_SIZE .mod PRIORITY_SLICE_SIZE)\nPRIORITY_DEF = $44\n"
        ".export runWait,waitDone,waitSnapshot,irqCompletionGeneration,runClear,clearStarted,runFinish,finishDone,irqSavedDcVideo,irqWorkKind,irqClearStage\n"
        ".export b8ClearPriorityRemaining,b8ClearPriorityAddress\n"
        "%.*s\n"
        "irqCompletionGeneration: .word $0\nwaitSnapshot: .word $0\n"
        "irqSavedDcVideo: .byte $0\nirqClearStage: .byte $0\nirqWorkKind: .byte $0\nsendIrqCommand: .byte $0\n"
        "runWait: WAIT_FOR_IRQ_COMPLETION waitSnapshot\nrts\nwaitDone: rts\n"
        "runClear:\ncustom_irq_handler:\n%.*s\n@servicePendingWork: rts\n@vSyncCounter: rts\nclearStarted: rts\n"
        "runFinish:\ncustom_clear_finish:\n%.*s\n@serviceClearBackground: rts\n@serviceClearMap: rts\n@serviceWorkReturn: rts\n"
        "%.*s\nfinishDone: rts\n",
        (int)(wait_end-wait),wait,(int)(clear_end-clear),clear,(int)(service_end-service),service,
        (int)(priority_end-priority),priority);
    CHECK(size>=0&&size<(int)sizeof(fixture),"IRQ fixture too large"); free(irq); free(graphics); write_file(source,fixture);
    write_file(config,"MEMORY { ZP:start=$22,size=$5C,file=\"\"; RAM:start=$2000,size=$6000,file=%O; }\nSEGMENTS { ZEROPAGE:load=ZP,type=zp,optional=yes; CODE:load=RAM,type=rw; RODATA:load=RAM,type=ro,optional=yes; DATA:load=RAM,type=rw,optional=yes; }\n");
    char *ca65[]={"ca65","-t","cx16","-I","src",source,"-o",object,NULL};
    char *ld65[]={"ld65","-C",config,object,"-o",binary,"-Ln",labels,NULL};
    run_tool("ca65",ca65); run_tool("ld65",ld65);
    FILE *file=fopen(binary,"rb"); CHECK(file,"Cannot read linked fixture"); CHECK(fread(ram+0x2000,1,0x6000,file)>0,"Fixture is empty"); fclose(file);
    file=fopen(labels,"r"); CHECK(file,"Cannot read labels"); char line[256],kind[16],name[100]; unsigned addr;
    while(fgets(line,sizeof(line),file)) if(sscanf(line,"%15s %x %99s",kind,&addr,name)==3) {
        CHECK(symbol_count<16,"Too many labels"); strcpy(symbols[symbol_count].name,name[0]=='.'?name+1:name); symbols[symbol_count++].address=addr;
    }
    fclose(file);
}
static void init_cpu(const char *entry,const char *done) {
    reset6502(false); regs.pc=(uint16_t)symbol(entry); regs.sp=0x1FD; regs.status=FLAG_CONSTANT;
    ram[0x1FE]=(uint8_t)(symbol(done)-1); ram[0x1FF]=(uint8_t)((symbol(done)-1)>>8); waiting=0; clockticks6502=0;
}
static void run_to_done(const char *entry,const char *done,unsigned limit) {
    init_cpu(entry,done);
    for(unsigned i=0;i<limit&&regs.pc!=symbol(done);++i) { CHECK(!waiting,"Unexpected WAI"); step6502(); }
    CHECK(regs.pc==symbol(done),"IRQ helper did not return");
}

static void test_completion_wait(void) {
    unsigned snap=symbol("waitSnapshot"),gen=symbol("irqCompletionGeneration");
    ram[snap]=0xFF; ram[snap+1]=0x12; ram[gen]=0; ram[gen+1]=0x13;
    run_to_done("runWait","waitDone",1000);
    CHECK(!(regs.status&FLAG_INTERRUPT),"Precompleted command did not return with IRQ enabled");
    ram[snap]=0xFF; ram[snap+1]=0xFF; ram[gen]=0; ram[gen+1]=0;
    run_to_done("runWait","waitDone",1000);
    CHECK(!waiting,"Completion generation rollover was not recognized");

    ram[snap]=0x34; ram[snap+1]=0x12; ram[gen]=0x34; ram[gen+1]=0x12;
    init_cpu("runWait","waitDone"); unsigned wakes=0;
    for(unsigned i=0;i<10000&&regs.pc!=symbol("waitDone");++i) {
        if(!waiting) { step6502(); continue; }
        CHECK(regs.status&FLAG_INTERRUPT,"Completion wait must mask IRQ while checking");
        struct regs before=regs; irq6502();
        CHECK(!waiting&&regs.pc==before.pc&&regs.sp==before.sp,"Masked IRQ did more than wake WAI");
        if(++wakes==2) { ram[gen]=0x35; ram[gen+1]=0x12; }
    }
    CHECK(regs.pc==symbol("waitDone")&&wakes==2,"Completion wait returned early or missed completion");
    CHECK(!(regs.status&FLAG_INTERRUPT),"In-progress completion wait did not enable IRQs on return");
}

static void test_clear_video_restore(void) {
    unsigned saved=symbol("irqSavedDcVideo"),work=symbol("irqWorkKind"),stage=symbol("irqClearStage"),gen=symbol("irqCompletionGeneration"),remaining=symbol("b8ClearPriorityRemaining"),cursor=symbol("b8ClearPriorityAddress");
    vera_ctrl=0x02; dc_video=0xF3; dc_hstart=0xA2; ram[saved]=ram[work]=0; ram[gen]=0xFF; ram[gen+1]=0x34;
    fx_ctrl=0; memset(fx_cache,0,sizeof(fx_cache)); memset(vram,0x5A,sizeof(vram)); memset(addr,0,sizeof(addr)); memset(stride,0,sizeof(stride));
    run_to_done("runClear","clearStarted",1000);
    CHECK(ram[saved]==0xF3&&dc_video==0x83,"Clear start did not blank only layers through DCSEL0");
    CHECK(vera_ctrl==0&&dc_hstart==0xA2,"Clear start changed DCSEL1 or left the selector set");
    CHECK(ram[work]==1&&ram[gen]==0xFF&&ram[gen+1]==0x34,"Clear start advanced completion before the slices finished");
    ram[stage]=2; ram[work]=1;
    ram[cursor]=(uint8_t)PRIORITY_TAIL_START; ram[cursor+1]=(uint8_t)(PRIORITY_TAIL_START>>8);
    ram[remaining]=(uint8_t)PRIORITY_TAIL_SIZE; ram[remaining+1]=(uint8_t)(PRIORITY_TAIL_SIZE>>8);
    data_writes=0;
    run_to_done("runFinish","finishDone",1000);
    CHECK(dc_video==0xF3&&vera_ctrl==0&&dc_hstart==0xA2,"Clear finish did not restore exact DC_VIDEO");
    CHECK(ram[work]==0&&ram[gen]==0&&ram[gen+1]==0x35,"Clear completion did not advance the generation exactly once");
    CHECK(data_writes==PRIORITY_TAIL_SIZE,"Final priority slice did not clear its exact tail");
    CHECK(fx_ctrl==0&&stride[0]==1&&vera_ctrl==0,"Final priority slice left FX mode, stride or control active");
    CHECK(addr[0]==PRIORITY_TAIL_START+PRIORITY_TAIL_SIZE,"Final priority slice left the VERA cursor at the wrong address");
    CHECK(ram[cursor]==(uint8_t)PRIORITY_TAIL_START&&ram[cursor+1]==(uint8_t)(PRIORITY_TAIL_START>>8),"Final priority slice changed the completed tail cursor");
    CHECK(ram[remaining]==(uint8_t)PRIORITY_TAIL_SIZE&&ram[remaining+1]==(uint8_t)(PRIORITY_TAIL_SIZE>>8),"Final priority slice changed the completed tail byte count");
    for(unsigned i=0;i<PRIORITY_TAIL_SIZE;++i) CHECK(vram[PRIORITY_TAIL_START+i]==0x44,"Final priority cache flush byte differs");
    CHECK(vram[PRIORITY_TAIL_START-1]==0x5A&&vram[PRIORITY_TAIL_START+PRIORITY_TAIL_SIZE]==0x5A,"Final priority slice cache flush crossed its exact range");
}

static void test_idle_generation_source(void) {
    char *irq=read_file("src/irqAsm.s"); char *idle=strstr(irq,"@noCommand:"); CHECK(idle,"Idle VSYNC branch missing");
    char *idle_end=strstr(idle,"\n@servicePendingWork:"); CHECK(idle_end,"Idle branch end missing");
    char saved=*idle_end; *idle_end=0;
    CHECK(!strstr(idle,"irqCompletionGeneration"),"Idle VSYNC increments command completion");
    *idle_end=saved;
    char *counter=strstr(irq,"@vSyncCounter:"); CHECK(counter,"VSYNC counter code missing");
    char *counter_end=strstr(counter,"@resetLatches:"); CHECK(counter_end,"VSYNC counter end missing");
    saved=*counter_end; *counter_end=0;
    CHECK(!strstr(counter,"irqCompletionGeneration"),"VSYNC counter path increments command completion"); free(irq);
}

int main(void) {
    suite_test_paths_init("irq-completion"); assemble();
    test_completion_wait(); test_clear_video_restore(); test_idle_generation_source();
    puts("IRQ completion handles precompleted/in-progress/wrap states; idle VSYNC is not completion; clear restores exact DC_VIDEO");
    return 0;
}
