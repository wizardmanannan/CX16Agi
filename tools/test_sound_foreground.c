/* Execute the real cc65-emitted foreground sound routines and PSG clear.
 * Requires a fresh tools/test-image.mk image; no game data or ROM required.
 * Build with the X16 emulator's src/cpu/fake6502.c and include directory.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "registers.h"
#include "fake6502.h"

extern struct regs regs;
static uint8_t ram[65536], bank_ram[8192], image[65536], bank_image[8192], vram[131072];
static unsigned vera_address, vera_stride, psg_writes, counter_writes, isr_writes;
static uint16_t counter, counter_high;
static struct { char name[100]; uint16_t address; } labels[8192];
static unsigned label_count;
#define CHECK(c,m) do { if (!(c)) { fprintf(stderr,"%s (%s:%d)\n",m,__FILE__,__LINE__); exit(1); } } while (0)

uint8_t read6502(uint16_t address, uint8_t bank) {
    (void)bank;
    if(address>=0xA000 && address<0xC000) {
        CHECK(ram[0]==0x0B,"Sound accessed the wrong RAM bank");
        return bank_ram[address-0xA000];
    }
    return ram[address];
}
void write6502(uint16_t address, uint8_t bank, uint8_t value) {
    (void)bank;
    if(address==counter || address==counter+1 || address==counter_high || address==counter_high+1) counter_writes++;
    if(address==0x9F27) { isr_writes++; ram[address]&=(uint8_t)~value; return; }
    if(address>=0x9F20 && address<=0x9F22) {
        static const unsigned increments[16]={0,1,2,4,8,16,32,64,128,256,512,40,80,160,320,640};
        ram[address]=value;
        vera_address=ram[0x9F20]|((unsigned)ram[0x9F21]<<8)|((unsigned)(ram[0x9F22]&1)<<16);
        vera_stride=increments[ram[0x9F22]>>4]; return;
    }
    if(address==0x9F23) {
        CHECK(vera_address>=0x1F9C0 && vera_address<0x1FA00,"PSG clear wrote outside PSG registers");
        vram[vera_address]=value; psg_writes++; vera_address=(vera_address+vera_stride)&0x1FFFF; return;
    }
    if(address>=0xA000 && address<0xC000) {
        CHECK(ram[0]==0x0B,"Sound wrote the wrong RAM bank");
        bank_ram[address-0xA000]=value;
    }
    else ram[address]=value;
}
uint8_t memory_get_ram_bank(void) { return ram[0]; }
uint8_t memory_get_rom_bank(void) { return ram[1]; }
void vp6502(void) {}
void stop6502(uint16_t address,uint8_t bank) { (void)address;(void)bank;CHECK(false,"CPU stopped"); }
static uint16_t symbol(const char *name) {
    for(unsigned i=0;i<label_count;i++) if(!strcmp(name,labels[i].name)) return labels[i].address;
    fprintf(stderr,"Missing production symbol %s\n",name);exit(1);
}
static void path(char *dest,size_t size,const char *build,const char *name) {
    CHECK(snprintf(dest,size,"%s/%s",build,name)<(int)size,"Build path too long");
}
static void load_image(const char *build) {
    char filename[4096],line[256],kind[16],name[100];unsigned address;
    path(filename,sizeof(filename),build,"final.lbl"); FILE *f=fopen(filename,"r"); CHECK(f,"Cannot open final.lbl");
    while(fgets(line,sizeof(line),f)) if(sscanf(line,"%15s %x %99s",kind,&address,name)==3) {
        CHECK(label_count < sizeof(labels) / sizeof(labels[0]) && address < 65536,"Invalid label file");
        strcpy(labels[label_count].name,name[0]=='.'?name+1:name); labels[label_count++].address=(uint16_t)address;
    }
    fclose(f);
    path(filename,sizeof(filename),build,"agi.cx16");f=fopen(filename,"rb");CHECK(f,"Cannot open resident image");
    int low=fgetc(f),high=fgetc(f);CHECK(low>=0 && high>=0,"Missing load address");
    unsigned start=(unsigned)low|((unsigned)high<<8);size_t n=fread(image+start,1,sizeof(image)-start,f);
    CHECK(n>0 && feof(f) && !ferror(f),"Resident image too large or unreadable");fclose(f);
    path(filename,sizeof(filename),build,"agi.cx16.0B");f=fopen(filename,"rb");CHECK(f,"Cannot open sound bank image");
    /* Bank files are raw segments (no PRG load-address header). */
    n=fread(bank_image,1,sizeof(bank_image),f);
    CHECK(n>0 && feof(f) && !ferror(f),"Sound bank image too large or unreadable");fclose(f);
    counter=symbol("_vSyncCounter"); counter_high=symbol("_vSyncCounterHigh");
}
static void put(uint16_t address,uint8_t value) {
    if(address>=0xA000 && address<0xC000) bank_ram[address-0xA000]=value;else ram[address]=value;
}
static uint8_t get(uint16_t address) { return read6502(address,0); }
static void word(uint16_t address,uint16_t value) { put(address,(uint8_t)value);put(address+1,(uint8_t)(value>>8)); }
static uint16_t getword(uint16_t address) { return get(address)|((uint16_t)get(address+1)<<8); }
static void prepare(bool masked) {
    memcpy(ram,image,sizeof(ram));memcpy(bank_ram,bank_image,sizeof(bank_ram));memset(vram,0xA5,sizeof(vram));
    reset6502(false);regs.sp=0x1FD;regs.a=regs.x=regs.y=regs.db=regs.k=0;
    regs.status=FLAG_CONSTANT|(masked?FLAG_INTERRUPT:0);waiting=0;clockticks6502=0;
    ram[0]=0x0B;ram[1]=0x37;ram[0x9F27]=1;
    ram[0x1FE]=0xFF;ram[0x1FF]=0x01; /* RTS -> sentinel $0200. */
    word(symbol("c_sp"),0x9000);word(symbol("_flag"),0x8800);word(counter,0x93B7);word(counter_high,0x6AD1);
    counter_writes=isr_writes=psg_writes=0;
}
static void call(const char *name,bool masked) {
    regs.pc=symbol(name);unsigned steps=0,masked_cycles=0,max_masked=0;
    while(regs.pc!=0x0200) {
        CHECK(++steps<1000000,"Sound caller did not return");unsigned before=clockticks6502;
        bool was_masked=(regs.status&FLAG_INTERRUPT)!=0;step6502();
        if(was_masked) masked_cycles+=clockticks6502-before;else masked_cycles=0;
        if(masked_cycles>max_masked) max_masked=masked_cycles;
    }
    CHECK((bool)(regs.status&FLAG_INTERRUPT)==masked,"Sound caller changed caller I flag");
    CHECK(regs.sp==0x1FF,"Sound caller leaked hardware stack");
    CHECK(getword(symbol("c_sp"))==(!strcmp(name,"_bBPlaySound")?0x9001:0x9000),"Sound caller corrupted the C argument stack");
    CHECK(ram[0]==0x0B && ram[1]==0x37,"Sound caller changed RAM/ROM banks");
    CHECK(!counter_writes && getword(counter)==0x93B7 && getword(counter_high)==0x6AD1,"Foreground sound changed serviced VSYNC counter");
    CHECK(!isr_writes && ram[0x9F27]==1,"Foreground sound discarded pending VSYNC");
    if(!masked) CHECK(max_masked<134400,"Sound critical section exceeds one VSYNC period");
}
static void cleared(void) {
    CHECK(psg_writes==64,"PSG clear did not write exactly 64 registers");
    for(unsigned i=0x1F9C0;i<0x1FA00;i++) CHECK(vram[i]==0,"PSG register remained audible");
}
/* Re-enter as a C caller without resetting playback or PSG state. */
static void rearm_call(void) {
    regs.sp=0x1FD;
    ram[0x1FE]=0xFF;ram[0x1FF]=0x01;
    word(symbol("c_sp"),0x9000);
    psg_writes=0;
}
int main(int argc,char **argv) {
    CHECK(argc==3 && !strcmp(argv[1],"--build"),"Usage: test_sound_foreground --build DIR");load_image(argv[2]);
    const char *ticks[]={"_bBCh1Ticks","_bBCh2Ticks","_bBCh3Ticks","_bBCh4Ticks"};
    const char *notes[]={"_ZP_CURRENTLY_PLAYING_NOTE_1","_ZP_CURRENTLY_PLAYING_NOTE_2","_ZP_CURRENTLY_PLAYING_NOTE_3","_ZP_CURRENTLY_PLAYING_NOTE_NOISE"};
    for(unsigned masked=0;masked<2;masked++) {
        prepare(masked);memset(bank_ram+symbol("_bBLoadedSounds")-0xA000,0xA5,(unsigned)(symbol("_bBLoadedSoundsPointer")-symbol("_bBLoadedSounds")));
        memset(bank_ram+symbol("_bBLoadedSoundsPointer")-0xA000,0xA5,256*2);
        for(unsigned i=0;i<4;i++)put(symbol("_bBIsPlaying")+i,1);
        put(symbol("_bBChannelsPlaying"),4);call("_bBInitSound",masked);cleared();
        for(unsigned i=0;i<(unsigned)(symbol("_bBLoadedSoundsPointer")-symbol("_bBLoadedSounds"));i++) CHECK(get(symbol("_bBLoadedSounds")+i)==0,"Init did not clear loaded sound entries");
        for(unsigned i=0;i<256*2;i++) CHECK(get(symbol("_bBLoadedSoundsPointer")+i)==0,"Init did not clear sound pointers");
        CHECK(!get(symbol("_bBChannelsPlaying")),"Init retained channels");
        for(unsigned i=0;i<4;i++)CHECK(!get(symbol("_bBIsPlaying")+i),"Init retained playback flag");

        prepare(masked);uint16_t sound=symbol("_bBLoadedSounds");
        word(symbol("_bBLoadedSoundsPointer")+14,sound); /* Sound 7. */
        for(unsigned i=0;i<4;i++) {word(sound+2*i,0xB000+0x100*i);word(symbol(ticks[i]),0xABCD);}
        put(sound+8,0x19);put(0x8800+9,1);put(0x8800+23,1);
        ram[0x9000]=7;regs.a=23;call("_bBPlaySound",masked);
        CHECK(!get(0x8800+23) && get(symbol("_bBEndSoundFlag"))==23,"Play end flag incorrect");
        CHECK(get(symbol("_bBSoundDataBank"))==0x19 && get(symbol("_bBChannelsPlaying"))==4,"Play channel/bank state incorrect");
        for(unsigned i=0;i<4;i++) {CHECK(!getword(symbol(ticks[i])),"Play retained ticks");CHECK(getword(symbol(notes[i]))==0xB000+0x100*i-5,"Play note pointer incorrect");CHECK(get(symbol("_bBIsPlaying")+i)==1,"Play channel not active");}
        CHECK(!psg_writes,"Play unexpectedly cleared PSG");

        /* Match command sound's end-flag publication, then exercise a real
         * stop/repeated-stop/restart/reset sequence on the same state. */
        word(symbol("_soundEndFlag"),23);
        rearm_call();call("_bBStopSound",masked);cleared();
        CHECK(get(0x8800+23)==1 && !get(symbol("_bBChannelsPlaying")),
              "Stop after play did not finish the published sound");
        rearm_call();call("_bBStopSound",masked);
        CHECK(!psg_writes,"Repeated stop rewrote idle PSG registers");
        rearm_call();ram[0x9000]=7;regs.a=31;word(symbol("_soundEndFlag"),31);
        call("_bBPlaySound",masked);
        CHECK(!get(0x8800+31) && get(0x8800+23)==1 && get(symbol("_bBChannelsPlaying"))==4,
              "Restart retained stale end flags or channel accounting");
        rearm_call();call("_bBInitSound",masked);cleared();
        CHECK(!get(symbol("_bBChannelsPlaying")),"Reset during playback retained channels");
        rearm_call();call("_bBInitSound",masked);cleared();
        CHECK(!get(symbol("_bBChannelsPlaying")),"Repeated reset retained channels");

        prepare(masked);word(symbol("_bBLoadedSoundsPointer")+255*2,0);
        word(symbol("_bBLoadedSoundsPointer")+14,sound);
        for(unsigned i=0;i<4;i++)word(sound+2*i,0xB000+0x100*i);
        put(sound+8,0x19);put(0x8800+9,1);put(0x8800+31,0);
        ram[0x9000]=7;regs.a=23;call("_bBPlaySound",masked);
        rearm_call();ram[0x9000]=255;regs.a=31;call("_bBPlaySound",masked);
        CHECK(get(0x8800+31)==1 && get(symbol("_bBChannelsPlaying"))==4 && !psg_writes,
              "Unloaded sound published playback state instead of rejecting the request");
        CHECK(get(symbol("_bBSoundDataBank"))==0x19 && get(symbol("_bBEndSoundFlag"))==23 && !get(0x8800+23),
              "Unloaded sound changed the previous sound's bank or end flag");
        for(unsigned i=0;i<4;i++)CHECK(!getword(symbol(ticks[i])) && get(symbol("_bBIsPlaying")+i)==1 &&
                                     getword(symbol(notes[i]))==0xB000+0x100*i-5,
                                     "Unloaded sound changed live channel state");

        prepare(masked);put(0x8800+9,0);put(symbol("_bBChannelsPlaying"),2);ram[0x9000]=7;regs.a=23;
        call("_bBPlaySound",masked);CHECK(get(0x8800+23)==1 && get(symbol("_bBChannelsPlaying"))==2 && !psg_writes,"Disabled sound play changed playback state");
        for(unsigned active=0;active<2;active++) {
            prepare(masked);word(symbol("_soundEndFlag"),23);put(0x8800+23,0);put(symbol("_bBChannelsPlaying"),active?4:0);
            for(unsigned i=0;i<4;i++)put(symbol("_bBIsPlaying")+i,(uint8_t)active);
            call("_bBStopSound",masked);CHECK(!get(symbol("_bBChannelsPlaying")),"Stop retained channels");
            for(unsigned i=0;i<4;i++)CHECK(!get(symbol("_bBIsPlaying")+i),"Stop retained playing flag");
            CHECK(get(0x8800+23)==active,"Stop end flag incorrect");if(active)cleared();else CHECK(!psg_writes,"Idle stop cleared PSG");
        }
    }
    puts("Production sound callers pass: init/play/stop, pending VSYNC, counter ownership, caller I/banks, PSG bounds and channel/end flags");return 0;
}
