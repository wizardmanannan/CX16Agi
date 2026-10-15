#define _GNU_SOURCE
/* Host runner: cc -O2 -Wall -Wextra tools/test_full_game_irq.c -lpng -o test_full_game_irq
 * Requires cmake, a C/C++ compiler, SDL2 development files, ImageMagick and libpng.
 * Instruments a temporary emulator copy. Hooks cost zero emulated CPU cycles.
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <png.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "test_run_from.h"

static void fail(const char *fmt, ...) {
    va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap); fputc('\n',stderr); exit(1);
}
#define REQUIRE(c,m) do { if (!(c)) fail("%s",m); } while(0)
static char *format(const char *fmt, ...) {
    char *s; va_list ap; va_start(ap,fmt); if(vasprintf(&s,fmt,ap)<0) fail("Out of memory"); va_end(ap); return s;
}
static char *join(const char *a,const char *b) { return format("%s/%s",a,b); }
static char *read_file(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb"); if(!f) fail("Read %s: %s",path,strerror(errno));
    if(fseek(f,0,SEEK_END)) fail("Seek %s",path);
    long n=ftell(f); REQUIRE(n>=0,"Invalid file size"); rewind(f);
    char *s=malloc((size_t)n+1); REQUIRE(s,"Out of memory"); REQUIRE(fread(s,1,(size_t)n,f)==(size_t)n,"Short read"); fclose(f); s[n]=0; if(length) *length=(size_t)n; return s;
}
static void write_file(const char *path,const void *data,size_t size) {
    FILE *f=fopen(path,"wb"); if(!f) fail("Write %s: %s",path,strerror(errno)); REQUIRE(fwrite(data,1,size,f)==size,"Short write"); REQUIRE(!fclose(f),"Close failed");
}
static bool exists(const char *path) { struct stat st; return !stat(path,&st); }
static void mkdirs(const char *path) {
    char *s=strdup(path); REQUIRE(s,"Out of memory"); for(char *p=s+1;*p;p++) if(*p=='/') { *p=0; if(mkdir(s,0700)&&errno!=EEXIST) fail("mkdir %s: %s",s,strerror(errno)); *p='/'; }
    if(mkdir(s,0700)&&errno!=EEXIST) fail("mkdir %s: %s",s,strerror(errno));
    free(s);
}
static void copy_file(const char *from,const char *to) {
    size_t n; char *s=read_file(from,&n); write_file(to,s,n); free(s); struct stat st; if(!stat(from,&st)) chmod(to,st.st_mode&0777);
}
/* Skip only the same generated directories/object files as the original runner. */
static void copy_tree(const char *from,const char *to) {
    mkdirs(to); DIR *d=opendir(from); if(!d) fail("opendir %s: %s",from,strerror(errno)); struct dirent *e;
    while((e=readdir(d))) {
        size_t n=strlen(e->d_name); if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||!strcmp(e->d_name,".git")||!strcmp(e->d_name,"build")||(n>=2&&!strcmp(e->d_name+n-2,".o"))) continue;
        char *a=join(from,e->d_name),*b=join(to,e->d_name); struct stat st; if(stat(a,&st)) fail("stat %s",a);
        if(S_ISDIR(st.st_mode)) copy_tree(a,b); else if(S_ISREG(st.st_mode)) copy_file(a,b); free(a);free(b);
    } closedir(d);
}
static void copy_stage(const char *from,const char *to,bool build) {
    DIR *d=opendir(from); if(!d) fail("opendir %s",from); struct dirent *e;
    while((e=readdir(d))) { bool agi=!strncasecmp(e->d_name,"agi.cx16",8); if(agi!=build) continue;
        char *a=join(from,e->d_name),*b=join(to,e->d_name); struct stat st; if(!stat(a,&st)&&S_ISREG(st.st_mode)) copy_file(a,b); free(a);free(b);
    } closedir(d);
}
static char *quoted(const char *s) {
    size_t n=strlen(s); char *out=malloc(n*6+3),*p=out; REQUIRE(out,"Out of memory"); *p++='"';
    for(const unsigned char *q=(const unsigned char*)s;*q;q++) { if(*q=='"'||*q=='\\') { *p++='\\';*p++=(char)*q; } else if(*q<32) { sprintf(p,"\\u%04x",*q);p+=6; } else *p++=(char)*q; } *p++='"';*p=0;return out;
}
/* JSON escapes for control bytes are not valid C universal character names. */
static char *quoted_c(const char *s) {
    size_t n=strlen(s); char *out=malloc(n*4+3),*p=out; REQUIRE(out,"Out of memory"); *p++='"';
    for(const unsigned char *q=(const unsigned char*)s;*q;q++) {
        if(*q=='"'||*q=='\\') { *p++='\\';*p++=(char)*q; }
        else if(*q<32||*q==127) { sprintf(p,"\\%03o",*q);p+=4; }
        else *p++=(char)*q;
    }
    *p++='"';*p=0;return out;
}
static void patch(const char *path,const char *old,const char *replacement,int expected) {
    char *s=read_file(path,NULL); size_t a=strlen(old),b=strlen(replacement); int count=0; char *p=s;
    while((p=strstr(p,old))) { count++;p+=a; } if(count!=expected) fail("Patch mismatch %s: expected %d, got %d",path,expected,count);
    size_t n=strlen(s); char *out=malloc(n+(b>a?(b-a)*(size_t)count:0)+1),*dest=out; REQUIRE(out,"Out of memory");p=s;char *match;
    while((match=strstr(p,old))) { size_t prefix=(size_t)(match-p);memcpy(dest,p,prefix);dest+=prefix;memcpy(dest,replacement,b);dest+=b;p=match+a; } strcpy(dest,p);write_file(path,out,strlen(out));free(s);free(out);
}

static const char hook[] =
"\n"
"static FILE *shake_csv, *shake_commands;\n"
"static uint32_t shake_vsync, shake_accept, shake_custom, shake_first_write, shake_last_write;\n"
"static uint32_t shake_entry_vsync, shake_calls, shake_psg_writes, shake_window_writes, shake_audible_writes, shake_sprite_writes;\n"
"static uint8_t shake_input_chars, shake_saved_input[40];\n"
"static bool shake_keyboard_queued, shake_input_replaced;\n"
"static uint32_t shake_frames_seen, shake_start_frames;\n"
"static uint8_t shake_entry_count, shake_entry_phase, shake_entry_channels;\n"
"static bool shake_irq_active, shake_injected;\n"
"static int shake_command_index;\n"
"static uint8_t shake_saved_window[50], shake_saved_cursor[2], shake_saved_pointer[2];\n"
"static uint32_t shake_command_start, shake_capture_end, shake_total_writes, shake_command_writes;\n"
"static uint16_t shake_return_pc, shake_return_sp;\n"
"static uint8_t shake_return_a,shake_return_x,shake_return_y,shake_return_bank,shake_return_rom;\n"
"static uint8_t shake_return_status;\n"
"static uint8_t shake_saved_display[5], shake_helper_ctrl;\n"
"static bool shake_command_entered, shake_helper_entered, shake_write_pending, shake_capture_started;\n"
"static uint32_t shake_capture_start_frames, shake_last_command_frames, shake_key_processed_frame;\n"
"static bool shake_keyboard_processed;\n"
"static FILE *foreground_csv;\n"
"static bool foreground_active, foreground_irq_active;\n"
"static int foreground_kind;\n"
"static uint16_t foreground_return_pc, foreground_irq_return_pc;\n"
"static uint8_t foreground_return_sp, foreground_irq_return_sp, foreground_entry_i;\n"
"static uint32_t foreground_start_cycles, foreground_start_frames, foreground_start_counter;\n"
"static uint32_t foreground_irq_start_cycles, foreground_irq_max_cycles, foreground_irq_count;\n"
"static uint8_t foreground_irq_a, foreground_irq_x, foreground_irq_y, foreground_irq_status;\n"
"static uint8_t foreground_irq_ram_bank, foreground_irq_rom_bank, foreground_irq_vera[4], foreground_irq_depth;\n"
"static uint8_t foreground_irq_zp[0x52+0x57], foreground_irq_work[500], foreground_irq_i2c[8];\n"
"static uint8_t foreground_max_irq_depth;\n"
"static FILE *picture_csv;\n"
"static bool picture_irq_suppressed, picture_draw_active;\n"
"static int picture_test_stage, picture_draw_count, picture_floodfill_count, picture_split_count;\n"
"static unsigned picture_load_entries, picture_draw_entries;\n"
"static uint8_t picture_saved_window[50], picture_saved_cursor[2], picture_saved_pointer[2], picture_saved_end[2];\n"
"static uint16_t picture_var0_address;\n"
"static uint8_t picture_saved_var0;\n"
"static uint8_t picture_capture_bytes[51840];\n"
"static FILE *blit_failure_csv;\n"
"static uint8_t blit_saved_allocator[2160];\n"
"static uint8_t blit_saved_allocator_code[4];\n"
"static bool blit_failure_active, blit_set_loop_entered, blit_set_loop_returned, blit_second_allocator_entered;\n"
"static bool blit_set_loop_succeeded, blit_set_loop_bank_restored, blit_allocator_state_saved;\n"
"static bool blit_pending_parent_phase;\n"
"static bool blit_saved_view_flags, blit_loaded_resource;\n"
"static bool blit_saved_stale_counter;\n"
"static uint8_t blit_view_flags[2], blit_loaded_view_num;\n"
"static uint8_t blit_stale_counter;\n"
"static unsigned blit_load_view_entries, blit_set_view_entries, blit_draw_entries;\n"
"static int blit_failure_stage;\n"
"static uint16_t blit_return_pc, blit_set_loop_return_pc;\n"
"static uint8_t blit_return_sp, blit_set_loop_return_sp, blit_entry_bank, blit_entry_rom, blit_entry_i, blit_set_loop_entry_bank;\n"
"static uint8_t blit_entry_depth, blit_result, blit_agi_return_depth, blit_parent_return_depth, blit_nested_depth;\n"
"static bool blit_bank_preserved, blit_rom_preserved, blit_i_preserved;\n"
"extern void shake_get_display(uint8_t *values);\n"
"extern void shake_get_i2c_state(uint8_t *values);\n"
"static void blit_restore_viewtab_flags(void);\n"
"static const uint8_t shake_commands_data[][3] = {{98,0,0},{99,0,240},{110,1,0},{110,0,0},{110,2,0}};\n"
"static const int shake_command_lengths[] = {2,3,2,2,2};\n"
"static uint8_t shake_read(uint16_t address) { return debug_read6502(address,0,USE_CURRENT_X16_BANK); }\n"
"static void shake_ram(uint16_t address,uint8_t value) { write6502(address,0,value); }\n"
"static uint32_t foreground_vsync_counter(void) { return (uint32_t)shake_read(S__vSyncCounter) | ((uint32_t)shake_read(S__vSyncCounter+1)<<8) | ((uint32_t)shake_read(S__vSyncCounterHigh)<<16) | ((uint32_t)shake_read(S__vSyncCounterHigh+1)<<24); }\n"
"static uint8_t foreground_vera_state(uint8_t index) { return index==0 ? video_read(5,true) : video_read(index-1,true); }\n"
"int shake_suppress_irq(void) { return picture_irq_suppressed && shake_read(S__foregroundVeraDepth)!=0; }\n"
"static uint32_t picture_capture(const char *path) {\n"
"    FILE *f=fopen(path,\"wb\"); if(!f) { perror(path); exit(2); }\n"
"    uint32_t hash=2166136261u;\n"
"    for(uint32_t i=0;i<sizeof(picture_capture_bytes);i++) { picture_capture_bytes[i]=video_space_read(i); hash=(hash^picture_capture_bytes[i])*16777619u; }\n"
"    if(fwrite(picture_capture_bytes,1,sizeof(picture_capture_bytes),f)!=sizeof(picture_capture_bytes) || fclose(f)) { fprintf(stderr,\"Could not write picture VRAM capture\\n\"); exit(2); }\n"
"    return hash;\n"
"}\n"
"static void picture_save_injection(const uint8_t *code,uint8_t length) {\n"
"    for(int i=0;i<50;i++) picture_saved_window[i]=shake_read(S_codeWindow+i);\n"
"    for(int i=0;i<2;i++) { picture_saved_cursor[i]=shake_read(S_cwCurrentCode+i); picture_saved_pointer[i]=shake_read(0xa9+i); shake_ram(S_cwCurrentCode+i,0); }\n"
"    for(int i=0;i<2;i++) picture_saved_end[i]=shake_read(S_endPos+i);\n"
"    uint16_t code_base=(uint16_t)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8)); uint16_t test_end=code_base+49; shake_ram(S_endPos,(uint8_t)test_end); shake_ram(S_endPos+1,(uint8_t)(test_end>>8));\n"
"    for(int i=0;i<length;i++) shake_ram(S_codeWindow+i,code[i]);\n"
"}\n"
"static void picture_restore_injection(void) {\n"
"    for(int i=0;i<50;i++) shake_ram(S_codeWindow+i,picture_saved_window[i]);\n"
"    for(int i=0;i<2;i++) { shake_ram(S_cwCurrentCode+i,picture_saved_cursor[i]); shake_ram(0xa9+i,picture_saved_pointer[i]); }\n"
"    for(int i=0;i<2;i++) shake_ram(S_endPos+i,picture_saved_end[i]);\n"
"}\n"
"static void picture_check_restored(bool check_var0) { for(int i=0;i<50;i++) if(shake_read(S_codeWindow+i)!=picture_saved_window[i]) { fprintf(stderr,\"Picture fixture did not restore code window\\n\"); exit(3); } for(int i=0;i<2;i++) if(shake_read(S_cwCurrentCode+i)!=picture_saved_cursor[i] || shake_read(0xa9+i)!=picture_saved_pointer[i] || shake_read(S_endPos+i)!=picture_saved_end[i]) { fprintf(stderr,\"Picture fixture did not restore interpreter pointers\\n\"); exit(3); } if(check_var0 && shake_read(picture_var0_address)!=picture_saved_var0) { fprintf(stderr,\"Picture fixture did not restore var[0]\\n\"); exit(3); } }\n"
"static void picture_inject_load_draw(void) { const uint8_t code[]={24,0,25,0}; picture_var0_address=(uint16_t)(shake_read(S__var)|((uint16_t)shake_read(S__var+1)<<8)); picture_saved_var0=shake_read(picture_var0_address); shake_ram(picture_var0_address,1); picture_load_entries=picture_draw_entries=0; picture_save_injection(code,sizeof(code)); picture_test_stage=1; }\n"
"static void picture_inject_reference_draw(void) { const uint8_t code[]={25,0}; shake_ram(picture_var0_address,1); picture_load_entries=picture_draw_entries=0; picture_save_injection(code,sizeof(code)); picture_irq_suppressed=true; picture_test_stage=3; }\n"
"static void foreground_begin(int kind) {\n"
"    if(FOREGROUND_TEST_ENABLED && !foreground_csv) { foreground_csv=fopen(FOREGROUND_CSV,\"w\"); if(!foreground_csv) { perror(FOREGROUND_CSV); exit(2); } fprintf(foreground_csv,\"caller_id,cycles,asserted_vsyncs,serviced_vsyncs,lost_vsyncs,busy_irq_count,max_busy_irq_cycles,entry_i\\n\"); }\n"
"    uint8_t sp=(uint8_t)regs.sp;\n"
"    uint16_t ret=(uint16_t)(shake_read(0x0100+(uint8_t)(sp+1))|((uint16_t)shake_read(0x0100+(uint8_t)(sp+2))<<8));\n"
"    foreground_kind=kind; foreground_return_pc=(uint16_t)(ret+1); foreground_return_sp=(uint8_t)(sp+2);\n"
"    foreground_start_cycles=clockticks6502; foreground_start_frames=shake_frames_seen; foreground_start_counter=foreground_vsync_counter();\n"
"    foreground_entry_i=(regs.status&FLAG_INTERRUPT)!=0; foreground_irq_count=foreground_irq_max_cycles=0; foreground_max_irq_depth=0; foreground_active=true;\n"
"    if(kind==1 && PICTURE_TEST_ENABLED && (picture_test_stage==1 || picture_test_stage==3)) { picture_draw_active=true; picture_floodfill_count=picture_split_count=0; }\n"
"}\n"
"static void foreground_finish(void) {\n"
"    uint32_t frames=shake_frames_seen-foreground_start_frames, serviced=foreground_vsync_counter()-foreground_start_counter;\n"
"    uint32_t missed=frames>serviced?frames-serviced:0; uint32_t cycles=clockticks6502-foreground_start_cycles;\n"
"    if(missed>0 && !(PICTURE_TEST_ENABLED && ((foreground_kind==1 && picture_irq_suppressed) || (foreground_kind==2 && picture_test_stage>=4)))) { fprintf(stderr,\"Production foreground caller %s lost %u VSYNC(s): cycles=%u asserted=%u serviced=%u\\n\",foreground_kind==1?\"b11DrawPic\":\"b9UpdateObjects\",missed,cycles,frames,serviced); exit(3); }\n"
"    if(shake_read(S__foregroundVeraDepth)) { fprintf(stderr,\"Foreground VERA ownership was not released after caller return\\n\"); exit(3); }\n"
"    if(PICTURE_TEST_ENABLED && foreground_kind==2 && blit_pending_parent_phase) { blit_parent_return_depth=shake_read(S__foregroundVeraDepth); fprintf(blit_failure_csv,\"%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\\n\",blit_failure_stage,blit_set_loop_entered,blit_set_loop_returned,blit_set_loop_succeeded,blit_set_loop_bank_restored,blit_second_allocator_entered,blit_result,blit_entry_depth,blit_nested_depth,blit_agi_return_depth,blit_parent_return_depth,blit_bank_preserved,blit_rom_preserved,blit_i_preserved,blit_load_view_entries,blit_set_view_entries,blit_draw_entries,blit_loaded_resource); fflush(blit_failure_csv); blit_pending_parent_phase=false; if(blit_failure_stage==1) blit_failure_stage=2; else { blit_restore_viewtab_flags(); emu_write(5,0); raise(SIGINT); } }\n"
"    fprintf(foreground_csv,\"%d,%u,%u,%u,%u,%u,%u,%u\\n\",foreground_kind,cycles,frames,serviced,missed,foreground_irq_count,foreground_irq_max_cycles,foreground_entry_i);\n"
"    fflush(foreground_csv); foreground_active=false; foreground_kind=0;\n"
"    if(picture_draw_active) {\n"
"        const char *path=picture_draw_count==0?PICTURE_IRQ_VRAM:PICTURE_REFERENCE_VRAM;\n"
"        uint32_t hash=picture_capture(path);\n"
"        fprintf(picture_csv,\"%d,1,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\\n\",picture_draw_count+1,cycles,frames,serviced,missed,foreground_irq_count,foreground_irq_max_cycles,foreground_max_irq_depth,picture_floodfill_count,picture_split_count,foreground_entry_i,hash,picture_load_entries,picture_draw_entries);\n"
"        fflush(picture_csv); picture_draw_count++; picture_draw_active=false; picture_irq_suppressed=false;\n"
"    }\n"
"}\n"
"void shake_irq_accepted(void) {\n"
"    if(foreground_active && shake_read(S__foregroundVeraDepth)) {\n"
"        foreground_irq_active=true; foreground_irq_start_cycles=clockticks6502;\n"
"        foreground_irq_return_pc=regs.pc; foreground_irq_return_sp=(uint8_t)regs.sp;\n"
"        foreground_irq_a=regs.a; foreground_irq_x=regs.xl; foreground_irq_y=regs.yl; foreground_irq_status=regs.status&0xcf;\n"
"        foreground_irq_ram_bank=memory_get_ram_bank(); foreground_irq_rom_bank=memory_get_rom_bank();\n"
"        foreground_irq_depth=shake_read(S__foregroundVeraDepth);\n"
"        if(!foreground_irq_depth) { fprintf(stderr,\"Foreground IRQ ran without VERA ownership\\n\"); exit(3); }\n"
"        if(foreground_irq_depth>foreground_max_irq_depth) foreground_max_irq_depth=foreground_irq_depth;\n"
"        for(int i=0;i<4;i++) foreground_irq_vera[i]=foreground_vera_state(i);\n"
"        for(int i=0;i<0x52;i++) foreground_irq_zp[i]=shake_read(0x22+i);\n"
"        for(int i=0;i<0x57;i++) foreground_irq_zp[0x52+i]=shake_read(0xa9+i);\n"
"        for(int i=0;i<500;i++) foreground_irq_work[i]=shake_read(0x602+i);\n"
"        shake_get_i2c_state(foreground_irq_i2c);\n"
"    }\n"
"    if(!shake_irq_active) { shake_accept=clockticks6502; shake_return_pc=regs.pc; shake_return_sp=regs.sp;\n"
"        shake_return_a=regs.a; shake_return_x=regs.xl; shake_return_y=regs.yl;\n"
"        shake_return_status=regs.status & 0xcf; // C/Z/I/D/V/N; exclude synthetic B/U.\n"
"        shake_return_bank=memory_get_ram_bank(); shake_return_rom=memory_get_rom_bank(); }\n"
"}\n"
"void shake_vsync_asserted(void) { shake_vsync=clockticks6502; shake_frames_seen++; }\n"
"void shake_foreground_vram_write(uint32_t address) { if(foreground_irq_active) { fprintf(stderr,\"KERNAL IRQ wrote VERA memory at $%05x during renderer ownership\\n\",address); exit(3); } }\n"
"void shake_foreground_vera_register_write(uint8_t reg,uint8_t dcsel) { if(foreground_irq_active && dcsel==0 && (reg<=2 || reg==5)) { fprintf(stderr,\"KERNAL IRQ changed VERA address/control register %u during renderer ownership\\n\",reg); exit(3); } }\n"
"void shake_window_write(void) {\n"
"    if(shake_irq_active) {\n"
"        shake_write_pending=true; shake_window_writes++; shake_total_writes++;\n"
"    }\n"
"}\n"
"void shake_psg_write(uint8_t reg,uint8_t value) { if(shake_irq_active) { shake_psg_writes++; if((reg&3)==2 && (value&63) && (value&192)) shake_audible_writes++; } }\n"
"void shake_sprite_write(void) { if(shake_irq_active) shake_sprite_writes++; }\n"
"static void shake_banked_ram(uint16_t address,uint8_t bank,uint8_t value) {\n"
"    uint8_t old=memory_get_ram_bank(); memory_set_ram_bank(bank); shake_ram(address,value); memory_set_ram_bank(old);\n"
"}\n"
"static uint8_t shake_banked_read(uint16_t address,uint8_t bank) { uint8_t old=memory_get_ram_bank(); memory_set_ram_bank(bank); uint8_t value=shake_read(address); memory_set_ram_bank(old); return value; }\n"
"static void blit_save_allocator(void) { for(int i=0;i<2160;i++) blit_saved_allocator[i]=shake_banked_read(S__bDSpriteAllocTable+i,13); blit_saved_allocator_code[0]=shake_banked_read(S_findFreeVRamLowByteLoop+1,13); blit_saved_allocator_code[1]=shake_banked_read(S_findFreeVRamLowByteLoop+2,13); blit_saved_allocator_code[2]=shake_banked_read(S_findFirstFreeVRamBlock_optimisticSkip,13); blit_saved_allocator_code[3]=shake_banked_read(S_findFirstFreeVRamBlock_optimisticSkip+1,13); blit_allocator_state_saved=true; }\n"
"static void blit_fill_allocator(uint8_t value) { for(int i=0;i<2160;i++) shake_banked_ram(S__bDSpriteAllocTable+i,13,value); }\n"
"static void blit_prepare_allocator(uint8_t value) { blit_fill_allocator(value); shake_banked_ram(S_findFreeVRamLowByteLoop+1,13,(uint8_t)S__bDSpriteAllocTable); shake_banked_ram(S_findFreeVRamLowByteLoop+2,13,(uint8_t)(S__bDSpriteAllocTable>>8)); shake_banked_ram(S_findFirstFreeVRamBlock_optimisticSkip,13,0xea); shake_banked_ram(S_findFirstFreeVRamBlock_optimisticSkip+1,13,0xea); }\n"
"static void blit_restore_allocator(void) { if(!blit_allocator_state_saved) return; for(int i=0;i<2160;i++) shake_banked_ram(S__bDSpriteAllocTable+i,13,blit_saved_allocator[i]); shake_banked_ram(S_findFreeVRamLowByteLoop+1,13,blit_saved_allocator_code[0]); shake_banked_ram(S_findFreeVRamLowByteLoop+2,13,blit_saved_allocator_code[1]); shake_banked_ram(S_findFirstFreeVRamBlock_optimisticSkip,13,blit_saved_allocator_code[2]); shake_banked_ram(S_findFirstFreeVRamBlock_optimisticSkip+1,13,blit_saved_allocator_code[3]); blit_allocator_state_saved=false; }\n"
"static void blit_save_viewtab_flags(void) { uint8_t flags_offset=shake_read(S__offsetOfFlags); uint16_t flags=(uint16_t)(S__viewtab+flags_offset); blit_view_flags[0]=shake_banked_read(flags,9); blit_view_flags[1]=shake_banked_read(flags+1,9); blit_saved_view_flags=true; }\n"
"static void blit_restore_viewtab_flags(void) { uint8_t flags_offset=shake_read(S__offsetOfFlags); uint16_t flags=(uint16_t)(S__viewtab+flags_offset); if(blit_saved_view_flags) { shake_banked_ram(flags,9,blit_view_flags[0]); shake_banked_ram(flags+1,9,blit_view_flags[1]); blit_saved_view_flags=false; } if(blit_saved_stale_counter) { uint16_t stale=(uint16_t)(S__viewtab+shake_read(S__offsetOfStopped)-1); shake_banked_ram(stale,9,blit_stale_counter); blit_saved_stale_counter=false; } }\n"
"static void picture_inject_blit_commands(void) { const uint8_t code[]={30,0,41,0,0,35,0}; blit_save_viewtab_flags(); blit_load_view_entries=blit_set_view_entries=blit_draw_entries=0; picture_save_injection(code,sizeof(code)); picture_test_stage=4; }\n"
"static uint16_t shake_call_return(uint8_t *return_sp) { uint8_t sp=(uint8_t)regs.sp; *return_sp=(uint8_t)(sp+2); return (uint16_t)(shake_read(0x0100+(uint8_t)(sp+1))|((uint16_t)shake_read(0x0100+(uint8_t)(sp+2))<<8))+1; }\n"
"static void blit_failure_before_step(uint16_t pc,uint8_t bank) {\n"
"    if(!PICTURE_TEST_ENABLED || picture_test_stage!=5 || regs.k || waiting) return;\n"
"    if(bank==9 && pc==S_b9UpdateObjects) { uint8_t flags_offset=shake_read(S__offsetOfFlags); uint16_t tab=S__viewtab; blit_loaded_view_num=shake_banked_read((uint16_t)(tab+shake_read(S__offsetOfCurrentView)),9); uint8_t flags=shake_banked_read((uint16_t)(tab+flags_offset),9); blit_loaded_resource=shake_banked_read((uint16_t)(S__loadedViews+blit_loaded_view_num*shake_read(S__sizeOfView)),0x11)!=0; if(!blit_loaded_resource) { fprintf(stderr,\"Native load.view did not load resource %u\\n\",blit_loaded_view_num); exit(3); } if(!(flags&1)) shake_banked_ram((uint16_t)(tab+flags_offset),9,(uint8_t)(flags|1)); uint16_t stale=(uint16_t)(tab+shake_read(S__offsetOfStopped)-1); if(!blit_saved_stale_counter) { blit_stale_counter=shake_banked_read(stale,9); blit_saved_stale_counter=true; } shake_banked_ram(stale,9,1); }\n"
"    if(foreground_active && foreground_kind==2 && !blit_pending_parent_phase && !blit_failure_active && pc==S__agiBlit && shake_read(S__foregroundVeraDepth)==1) {\n"
"        blit_failure_active=true; blit_set_loop_entered=blit_set_loop_returned=blit_second_allocator_entered=false; blit_set_loop_succeeded=blit_set_loop_bank_restored=false; blit_nested_depth=0;\n"
"        blit_entry_bank=bank; blit_entry_rom=memory_get_rom_bank(); blit_entry_i=(regs.status&FLAG_INTERRUPT)!=0; blit_entry_depth=shake_read(S__foregroundVeraDepth);\n"
"        blit_return_pc=shake_call_return(&blit_return_sp); blit_save_allocator(); blit_prepare_allocator(blit_failure_stage==1?1:0); regs.a=1;\n"
"    }\n"
"    if(!blit_failure_active) return;\n"
"    if(bank==0x0e && pc==S__bESetLoop && !blit_set_loop_entered) { blit_set_loop_entered=true; blit_set_loop_entry_bank=bank; blit_set_loop_return_pc=shake_call_return(&blit_set_loop_return_sp); uint8_t depth=shake_read(S__foregroundVeraDepth); if(depth>blit_nested_depth) blit_nested_depth=depth; }\n"
"    if(blit_set_loop_entered && !blit_set_loop_returned && pc==blit_set_loop_return_pc && (uint8_t)regs.sp==blit_set_loop_return_sp) { blit_set_loop_returned=true; blit_set_loop_succeeded=regs.a!=0; blit_set_loop_bank_restored=memory_get_ram_bank()==blit_set_loop_entry_bank; }\n"
"    if(bank==0x0e && pc==S__bEAllocateSpriteMemory && blit_failure_stage==2 && !blit_second_allocator_entered) { blit_second_allocator_entered=true; blit_prepare_allocator(1); uint8_t depth=shake_read(S__foregroundVeraDepth); if(depth>blit_nested_depth) blit_nested_depth=depth; }\n"
"    if(pc==blit_return_pc && (uint8_t)regs.sp==blit_return_sp) {\n"
"        blit_result=regs.a; blit_agi_return_depth=shake_read(S__foregroundVeraDepth); blit_bank_preserved=memory_get_ram_bank()==blit_entry_bank; blit_rom_preserved=memory_get_rom_bank()==blit_entry_rom; blit_i_preserved=((regs.status&FLAG_INTERRUPT)!=0)==blit_entry_i;\n"
"        bool branch_ok=blit_failure_stage==1 ? (blit_set_loop_entered && blit_set_loop_returned && !blit_set_loop_succeeded && blit_set_loop_bank_restored) : blit_second_allocator_entered;\n"
"        if(!(blit_entry_depth==1 && blit_nested_depth==2 && branch_ok && blit_result==0 && blit_agi_return_depth==blit_entry_depth && blit_bank_preserved && blit_rom_preserved && blit_i_preserved)) { fprintf(stderr,\"agiBlit phase %d failed: depth %u->%u->%u result=%u setloop=%u/%u/%u bank=%u alloc=%u bank/rom/i=%u/%u/%u\\n\",blit_failure_stage,blit_entry_depth,blit_nested_depth,blit_agi_return_depth,blit_result,blit_set_loop_entered,blit_set_loop_returned,blit_set_loop_succeeded,blit_set_loop_bank_restored,blit_second_allocator_entered,blit_bank_preserved,blit_rom_preserved,blit_i_preserved); exit(3); }\n"
"        blit_pending_parent_phase=true; blit_restore_allocator(); blit_failure_active=false;\n"
"    }\n"
"}\n"
"static void picture_test_before_step(uint16_t pc) {\n"
"    if(pc!=S_mainLoop || clockticks6502<TRIGGER_CYCLE || foreground_active) return;\n"
"    if(picture_test_stage==0) {\n"
"        picture_csv=fopen(PICTURE_CSV,\"w\"); if(!picture_csv) { perror(PICTURE_CSV); exit(2); }\n"
"        fprintf(picture_csv,\"draw,picture,cycles,asserted_vsyncs,serviced_vsyncs,lost_vsyncs,busy_irq_count,max_busy_irq_cycles,max_owner_depth,floodfill_sections,priority_splits,entry_i,vram_hash,load_pic_entries,draw_pic_entries\\n\");\n"
"        picture_inject_load_draw(); return;\n"
"    }\n"
"    if(picture_test_stage==1 && picture_draw_count==1 && shake_read(S_cwCurrentCode)==4 && !shake_read(S_cwCurrentCode+1)) {\n"
"        picture_restore_injection(); picture_check_restored(false); picture_test_stage=2; return;\n"
"    }\n"
"    if(picture_test_stage==2) { picture_inject_reference_draw(); return; }\n"
"    if(picture_test_stage==3 && picture_draw_count==2 && shake_read(S_cwCurrentCode)==2 && !shake_read(S_cwCurrentCode+1)) {\n"
"        picture_restore_injection(); shake_ram(picture_var0_address,picture_saved_var0); picture_check_restored(true);\n"
"        if(shake_read(S__foregroundVeraDepth)) { fprintf(stderr,\"Picture test left VERA ownership active\\n\"); exit(3); }\n"
"        blit_failure_csv=fopen(BLIT_FAILURE_CSV,\"w\"); if(!blit_failure_csv) { perror(BLIT_FAILURE_CSV); exit(2); }\n"
"        fprintf(blit_failure_csv,\"phase,setloop_entered,setloop_returned,setloop_success,setloop_bank_restored,second_allocator_entered,agi_result,entry_depth,nested_depth,agi_return_depth,parent_return_depth,bank_preserved,rom_preserved,i_preserved,load_view,set_view,draw_command,view_loaded\\n\"); fflush(blit_failure_csv);\n"
"        blit_failure_stage=1; picture_inject_blit_commands(); return;\n"
"    }\n"
"    if(picture_test_stage==4 && shake_read(S_cwCurrentCode)==7 && !shake_read(S_cwCurrentCode+1)) {\n"
"        picture_restore_injection(); picture_check_restored(true);\n"
"        if(blit_load_view_entries!=1 || blit_set_view_entries!=1 || blit_draw_entries!=1) { fprintf(stderr,\"Native view test commands did not execute load.view/set.view/draw: %u/%u/%u\\n\",blit_load_view_entries,blit_set_view_entries,blit_draw_entries); exit(3); }\n"
"        picture_test_stage=5; return;\n"
"    }\n"
"}\n"
"static void shake_before_step(void) {\n"
"    if(!shake_csv) {\n"
"        shake_csv=fopen(SHAKE_CSV,\"w\"); shake_commands=fopen(SHAKE_COMMANDS,\"w\");\n"
"        if(!shake_csv || !shake_commands) { perror(\"profile output\"); exit(2); }\n"
"        fprintf(shake_csv,\"cycle,irq_cycles,custom_cycles,accept_delay,custom_delay,count,phase,channels,calls,psg_writes,window_writes,first_write,last_write,audible_writes,sprite_writes,input_chars,hstart,hstop,vstart,vstop,border,phase_after,frames_after\\n\");\n"
"        fprintf(shake_commands,\"opcode,operand,entry_cycle,exit_cycle,vsyncs,display_restored,code_window_restored,cursor_restored,window_writes,actual_command_entered,kbd_queue\\n\");\n"
"    }\n"
"    if(regs.k || waiting) return;\n"
"    uint16_t pc=regs.pc; uint8_t bank=memory_get_ram_bank();\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage && bank==2 && pc==S__b2Load_pic) picture_load_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage && bank==2 && pc==S__b2Draw_pic) picture_draw_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage==4 && bank==2 && pc==S__b2Load_view) blit_load_view_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage==4 && bank==2 && pc==S__b2Set_view) blit_set_view_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage==4 && bank==2 && pc==S__b2Draw) blit_draw_entries++;\n"
"    if(foreground_irq_active && pc==foreground_irq_return_pc && (uint8_t)regs.sp==foreground_irq_return_sp) {\n"
"        uint32_t cycles=clockticks6502-foreground_irq_start_cycles; if(cycles>foreground_irq_max_cycles) foreground_irq_max_cycles=cycles; foreground_irq_count++;\n"
"        if(regs.a!=foreground_irq_a || regs.xl!=foreground_irq_x || regs.yl!=foreground_irq_y || (regs.status&0xcf)!=foreground_irq_status || memory_get_ram_bank()!=foreground_irq_ram_bank || memory_get_rom_bank()!=foreground_irq_rom_bank) { fprintf(stderr,\"Foreground IRQ changed CPU registers/flags/banks\\n\"); exit(3); }\n"
"        for(int i=0;i<4;i++) if(foreground_vera_state(i)!=foreground_irq_vera[i]) { fprintf(stderr,\"Foreground IRQ changed VERA address/control state\\n\"); exit(3); }\n"
"        for(int i=0;i<0x52;i++) if(shake_read(0x22+i)!=foreground_irq_zp[i]) { fprintf(stderr,\"Foreground IRQ changed zero-page scratch at $%02x\\n\",0x22+i); exit(3); }\n"
"        for(int i=0;i<0x57;i++) if(shake_read(0xa9+i)!=foreground_irq_zp[0x52+i]) { fprintf(stderr,\"Foreground IRQ changed zero-page scratch at $%02x\\n\",0xa9+i); exit(3); }\n"
"        for(int i=0;i<500;i++) if(shake_read(0x602+i)!=foreground_irq_work[i]) { fprintf(stderr,\"Foreground IRQ changed renderer work area at $%04x\\n\",0x602+i); exit(3); }\n"
"        uint8_t i2c_state[8]; shake_get_i2c_state(i2c_state); if(memcmp(i2c_state,foreground_irq_i2c,sizeof(i2c_state))) { fprintf(stderr,\"Foreground IRQ changed I2C protocol state\\n\"); exit(3); }\n"
"        foreground_irq_active=false;\n"
"    }\n"
"    if(FOREGROUND_TEST_ENABLED && !foreground_active) { if(bank==0x11 && pc==S__b11DrawPic) foreground_begin(1); else if(bank==9 && pc==S_b9UpdateObjects) foreground_begin(2); }\n"
"    if(foreground_active && pc==foreground_return_pc && (uint8_t)regs.sp==foreground_return_sp) foreground_finish();\n"
"    if(picture_draw_active) { if(pc==S__b8AsmFloodFillSections) picture_floodfill_count++; if(pc==S__b11SplitPriority) picture_split_count++; }\n"
"    if(PICTURE_TEST_ENABLED) blit_failure_before_step(pc,bank);\n"
"    if(PICTURE_TEST_ENABLED) { picture_test_before_step(pc); return; }\n"
"    if(shake_injected && shake_command_index==2 && !shake_keyboard_queued && clockticks6502-shake_command_start>200000) {\n"
"        static char key[]=\"A\"; machine_paste(key,false); shake_keyboard_queued=true;\n"
"    }\n"
"    if(pc==S_custom_irq_handler) {\n"
"        shake_irq_active=true; shake_custom=clockticks6502; shake_entry_vsync=shake_vsync;\n"
"        shake_calls=shake_psg_writes=shake_window_writes=shake_first_write=shake_last_write=shake_audible_writes=shake_sprite_writes=0;\n"
"        shake_input_chars=0; while(shake_read(S__inputLineDisplayed) && shake_input_chars<40 && debug_read6502(S__b7CurrentInputStr+shake_input_chars,0,7)) shake_input_chars++;\n"
"        shake_entry_count=shake_read(S_shakeCount); shake_entry_phase=shake_read(S_shakePhases);\n"
"        shake_entry_channels=debug_read6502(S__bBChannelsPlaying,0,11);\n"
"    }\n"
"    if(shake_irq_active) {\n"
"        if(pc==S_handleScreenShake) { shake_helper_entered=true; shake_helper_ctrl=video_read(5,true); }\n"
"        if(pc==S_local_handleSound && shake_helper_entered) {\n"
"            if(video_read(5,true)!=shake_helper_ctrl) { fprintf(stderr,\"Helper CTRL restoration failed\\n\"); exit(3); }\n"
"            shake_helper_entered=false;\n"
"        }\n"
"        if(bank==11 && pc==S_bBSoundHandler) shake_calls|=1;\n"
"        if(bank==7 && pc==S_b7HandleInputLine) shake_calls|=2;\n"
"        if(bank==10 && pc==S_bADisplayMenu) shake_calls|=4;\n"
"        if(bank==16 && pc==S_b10DisplayStatusBar) shake_calls|=8;\n"
"        if(bank==14 && pc==S_bEHandleSpriteUpdates) shake_calls|=16;\n"
"        if(bank==16 && pc==S_b10PrintStatusBarString) shake_calls|=32;\n"
"    }\n"
"    if(bank==4 && pc==S_b4ShakeScreen && shake_injected) shake_command_entered=true;\n"
"    if(pc!=S_mainLoop || shake_irq_active || clockticks6502<TRIGGER_CYCLE) return;\n"
"    if(!shake_capture_started) { warp_mode=false; emu_write(5,2); shake_capture_started=true; shake_capture_start_frames=shake_frames_seen; return; }\n"
"    if(shake_frames_seen-shake_capture_start_frames<16) return;\n"
"    if(shake_injected) {\n"
"        int length=shake_command_lengths[shake_command_index];\n"
"        if(shake_read(S_cwCurrentCode)!=length || shake_read(S_cwCurrentCode+1)) {\n"
"            fprintf(stderr,\"Injected command cursor mismatch\\n\"); exit(3);\n"
"        }\n"
"        uint8_t display[5]; shake_get_display(display);\n"
"        if(memcmp(display,shake_saved_display,5)!=0 || (shake_commands_data[shake_command_index][0]==110 &&\n"
"           (!shake_command_entered || shake_read(S_shakeCount) || shake_read(S_shakePhases) || shake_read(S_shakeFrames)))) {\n"
"            fprintf(stderr,\"Command/display restoration failed\\n\"); exit(3);\n"
"        }\n"
"        if(shake_input_replaced) {\n"
"            for(int i=0;i<40;i++) shake_banked_ram(S__b7CurrentInputStr+i,7,shake_saved_input[i]);\n"
"            shake_ram(S__inputLineDirty,1);\n"
"            shake_input_replaced=false;\n"
"        }\n"
"        for(int i=0;i<50;i++) shake_ram(S_codeWindow+i,shake_saved_window[i]);\n"
"        for(int i=0;i<2;i++) {\n"
"            shake_ram(S_cwCurrentCode+i,shake_saved_cursor[i]);\n"
"            shake_ram(0xa9+i,shake_saved_pointer[i]);\n"
"        }\n"
"        for(int i=0;i<50;i++) if(shake_read(S_codeWindow+i)!=shake_saved_window[i]) exit(3);\n"
"        for(int i=0;i<2;i++) if(shake_read(S_cwCurrentCode+i)!=shake_saved_cursor[i] || shake_read(0xa9+i)!=shake_saved_pointer[i]) exit(3);\n"
"        fprintf(shake_commands,\"%u,%u,%u,%u,%u,1,1,1,%u,%u,%u\\n\",shake_commands_data[shake_command_index][0],\n"
"            shake_commands_data[shake_command_index][1],shake_command_start,clockticks6502,shake_frames_seen-shake_start_frames,shake_total_writes-shake_command_writes,shake_command_entered,debug_read6502(NDX,0,0));\n"
"        fflush(shake_commands);\n"
"        shake_injected=false; shake_command_index++;\n"
"        if(shake_command_index==5) { shake_last_command_frames=shake_frames_seen; shake_capture_end=shake_frames_seen+120; return; }\n"
"    }\n"
"    if(shake_command_index==5) return;\n"
"    for(int i=0;i<50;i++) shake_saved_window[i]=shake_read(S_codeWindow+i);\n"
"    for(int i=0;i<2;i++) {\n"
"        shake_saved_cursor[i]=shake_read(S_cwCurrentCode+i);\n"
"        shake_saved_pointer[i]=shake_read(0xa9+i);\n"
"        shake_ram(S_cwCurrentCode+i,0);\n"
"    }\n"
"    for(int i=0;i<shake_command_lengths[shake_command_index];i++)\n"
"        shake_ram(S_codeWindow+i,shake_commands_data[shake_command_index][i]);\n"
"    if(shake_commands_data[shake_command_index][0]==110) {\n"
"        // Exercise real menu drawing for count1; real score/status redraw for count2.\n"
"        shake_ram(S__menuShown,shake_command_index==2);\n"
"        shake_ram(S__menuDirty,1);\n"
"        shake_ram(0x403,shake_read(0x403)+1);\n"
"        shake_ram(S__mainLoopCounter,shake_read(S__mainLoopCounter)+1);\n"
"        // Replay the retained game-produced sprite batch through its real IRQ.\n"
"        if(!(debug_read6502(S__bESpritesUpdatedBuffer,0,14)|debug_read6502(S__bESpritesUpdatedBuffer+1,0,14))) {\n"
"            fprintf(stderr,\"No real game sprite batch to replay\\n\"); exit(3);\n"
"        }\n"
"        shake_banked_ram(S__bESpritesUpdatedBufferPointer,14,(S__bESpritesUpdatedBuffer+8)&255);\n"
"        shake_banked_ram(S__bESpritesUpdatedBufferPointer+1,14,(S__bESpritesUpdatedBuffer+8)>>8);\n"
"        if(shake_command_index==4) {\n"
"            for(int i=0;i<40;i++) shake_saved_input[i]=debug_read6502(S__b7CurrentInputStr+i,0,7);\n"
"            const char input[]=\"LOOK\";\n"
"            for(int i=0;i<5;i++) shake_banked_ram(S__b7CurrentInputStr+i,7,input[i]);\n"
"            shake_ram(S__inputLineDirty,1);\n"
"            shake_input_replaced=true;\n"
"        }\n"
"    }\n"
"    shake_get_display(shake_saved_display); shake_command_writes=shake_total_writes; shake_command_entered=false;\n"
"    shake_command_start=clockticks6502; shake_start_frames=shake_frames_seen; shake_injected=true;\n"
"}\n"
"static void shake_after_step(uint8_t opcode) {\n"
"    if(shake_write_pending) {\n"
"        if(shake_window_writes==1) shake_first_write=clockticks6502-shake_entry_vsync;\n"
"        shake_last_write=clockticks6502-shake_entry_vsync; shake_write_pending=false;\n"
"    }\n"
"    if(shake_irq_active && opcode==0x40 && regs.pc==shake_return_pc && regs.sp==shake_return_sp) {\n"
"        if(regs.a!=shake_return_a || regs.xl!=shake_return_x || regs.yl!=shake_return_y ||\n"
"           (regs.status & 0xcf)!=shake_return_status ||\n"
"           memory_get_ram_bank()!=shake_return_bank || memory_get_rom_bank()!=shake_return_rom) {\n"
"            fprintf(stderr,\"Full IRQ corrupted foreground registers/flags/banks\\n\"); exit(3);\n"
"        }\n"
"        uint8_t display[5]; shake_get_display(display);\n"
"        fprintf(shake_csv,\"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\\n\",clockticks6502,\n"
"            clockticks6502-shake_accept,clockticks6502-shake_custom,\n"
"            shake_accept-shake_entry_vsync,shake_custom-shake_entry_vsync,\n"
"            shake_entry_count,shake_entry_phase,shake_entry_channels,shake_calls,\n"
"            shake_psg_writes,shake_window_writes,shake_first_write,shake_last_write,shake_audible_writes,shake_sprite_writes,shake_input_chars,\n"
"            display[0],display[1],display[2],display[3],display[4],shake_read(S_shakePhases),shake_read(S_shakeFrames));\n"
"        fflush(shake_csv); shake_irq_active=false;\n"
"    }\n"
"    if(!shake_keyboard_processed && shake_command_index==5 && debug_read6502(S__b7CurrentInputStr,0,7)=='a') {\n"
"        shake_keyboard_processed=true; shake_key_processed_frame=shake_frames_seen;\n"
"    }\n"
"    if(shake_capture_end && (shake_frames_seen>=shake_capture_end ||\n"
"       (shake_keyboard_processed && shake_frames_seen-shake_last_command_frames>=16 && shake_frames_seen-shake_key_processed_frame>=2))) {\n"
"        fprintf(stderr,\"Keyboard processed after resume: %u\\n\",shake_keyboard_processed);\n"
"        emu_write(5,0); raise(SIGINT); shake_capture_end=0;\n"
"    }\n"
"}\n"
;
    static const char *names[] = {"custom_irq_handler","mainLoop","codeWindow","cwCurrentCode","endPos","b4ShakeScreen","shakeCount","shakePhases","shakeFrames","_menuDirty","_menuShown","_mainLoopCounter","_bBChannelsPlaying","bBSoundHandler","b7HandleInputLine","bADisplayMenu","b10DisplayStatusBar","bEHandleSpriteUpdates","b10PrintStatusBarString","handleScreenShake","@handleSound","_inputLineDisplayed","_inputLineDirty","_b7CurrentInputStr","_bESpritesUpdatedBuffer","_bESpritesUpdatedBufferPointer","_b11DrawPic","_b2Load_pic","_b2Draw_pic","_b2Load_view","_b2Set_view","_b2Draw","b9UpdateObjects","_agiBlit","_bESetLoop","_bEAllocateSpriteMemory","_bDSpriteAllocTable","_viewtab","_loadedViews","_offsetOfCurrentView","_offsetOfEntryNum","_offsetOfFlags","_offsetOfStopped","_sizeOfViewTab","_sizeOfView","findFreeVRamLowByteLoop","findFirstFreeVRamBlock_optimisticSkip","_vSyncCounter","_vSyncCounterHigh","_foregroundVeraDepth","_b8AsmFloodFillSections","_b11SplitPriority","_var"};
#define NAME_COUNT (sizeof(names)/sizeof(names[0]))
static unsigned labels[NAME_COUNT]; static bool found[NAME_COUNT];
static void load_symbols(const char *path) {
    if(!exists(path)) return;
    char *data=read_file(path,NULL),*save;
    for(char *line=strtok_r(data,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
        char name[512];unsigned address;
        if(sscanf(line,"al %x %511s",&address,name)==2) {
            char *n=name;while(*n=='.')n++;
            for(size_t i=0;i<NAME_COUNT;i++)if(!strcmp(n,names[i])) { labels[i]=address;found[i]=true; }
        } else if(!strncmp(line,"sym\t",4)&&(strstr(line,"type=lab")||strstr(line,"type=equ"))) {
            char *n=strstr(line,"name=\""),*v=strstr(line,"val=0x"); if(!n||!v)continue; n+=6;char *end=strchr(n,'"');if(!end)continue;*end=0;
            address=(unsigned)strtoul(v+4,NULL,16);for(size_t i=0;i<NAME_COUNT;i++)if(!strcmp(n,names[i])) { labels[i]=address;found[i]=true; }
        }
    } free(data);
}
static uint64_t (*csv(const char *path,int columns,size_t *count))[23] {
    FILE *f=fopen(path,"r");if(!f)fail("Read %s",path);char *line=NULL;size_t len=0,n=0,capacity=256;
    uint64_t (*rows)[23]=malloc(capacity*sizeof(*rows));REQUIRE(rows,"Out of memory");REQUIRE(getline(&line,&len,f)>0,"Missing CSV header");
    while(getline(&line,&len,f)>0) { if(n==capacity) { capacity*=2;void *grown=realloc(rows,capacity*sizeof(*rows));REQUIRE(grown,"Out of memory");rows=grown; }
        char *p=line;for(int i=0;i<columns;i++) { char *end;errno=0;rows[n][i]=strtoull(p,&end,10);REQUIRE(end!=p&&!errno,"Invalid CSV number");REQUIRE(i==columns-1?(*end=='\n'||!*end):*end==',',"Invalid CSV column count");p=end+1; }n++;
    }free(line);fclose(f);*count=n;return rows;
}
static bool crop_equal(const unsigned char *a,const unsigned char *b,int width,int x,int y,int right,int bottom,int dx,int dy) {
    for(int row=y;row<bottom;row++) if(memcmp(a+((size_t)row*width+x)*3,b+((size_t)(row+dy)*width+x+dx)*3,(size_t)(right-x)*3))return false;
    return true;
}
static void save_png(const char *path,const unsigned char *pixels,unsigned width,unsigned height) {
    png_image image={0};image.version=PNG_IMAGE_VERSION;image.width=width;image.height=height;image.format=PNG_FORMAT_RGB;
    if(!png_image_write_to_file(&image,path,0,pixels,0,NULL))fail("PNG %s: %s",path,image.message);
}
static void validate_picture(const char *output) {
    size_t count;uint64_t (*rows)[23]=csv(join(output,"picture.csv"),15,&count);
    REQUIRE(count==2,"Picture test did not complete both production draws");
    REQUIRE(rows[0][0]==1&&rows[1][0]==2&&rows[0][1]==1&&rows[1][1]==1,"Picture test used an unexpected draw or resource");
    REQUIRE(rows[0][3]>0&&rows[0][3]==rows[0][4]&&!rows[0][5],"IRQ-enabled b11DrawPic did not service every asserted VSYNC");
    REQUIRE(rows[0][6]>0&&rows[0][8]==1&&rows[0][9]>0&&rows[0][10]>0,"IRQ-enabled b11DrawPic missed an owned IRQ, fill, or priority split");
    REQUIRE(rows[0][7]<4096,"Foreground picture IRQ exceeded its 4096-cycle budget");
    REQUIRE(!rows[0][11],"b11DrawPic entered with IRQs masked");
    REQUIRE(rows[1][3]>rows[1][4]&&rows[1][5]>0&&!rows[1][6]&&!rows[1][8],"IRQ-disabled reference did not suppress an owned VSYNC");
    REQUIRE(rows[1][9]>0&&rows[1][10]>0&&!rows[1][11],"Reference draw did not execute flood fill and priority split");
    REQUIRE(rows[0][13]==1&&rows[0][14]==1&&rows[1][13]==0&&rows[1][14]==1,"Picture fixture did not execute the real load.pic/draw.pic commands");

    size_t blit_count;uint64_t (*blit)[23]=csv(join(output,"agi-blit-failures.csv"),18,&blit_count);
    REQUIRE(blit_count==2&&blit[0][0]==1&&blit[1][0]==2,"Native run did not complete both agiBlit failure scenarios");
    REQUIRE(blit[0][1]&&blit[0][2]&&!blit[0][3]&&blit[0][4]&&!blit[0][6],"agiBlit bESetLoop failure did not return false with its bank restored");
    REQUIRE(blit[1][5]&&!blit[1][6],"agiBlit sprite allocation failure did not return false");
    for(size_t i=0;i<2;i++) {
        REQUIRE(blit[i][7]==1&&blit[i][8]==2&&blit[i][9]==1&&blit[i][10]==0,"agiBlit ownership nesting did not restore depths 1->2->1->0");
        REQUIRE(blit[i][11]&&blit[i][12]&&blit[i][13],"agiBlit failure changed RAM bank, ROM bank, or I flag");
        REQUIRE(blit[i][14]==1&&blit[i][15]==1&&blit[i][16]==1&&blit[i][17]==1,"agiBlit fixture did not load, set, and draw a real view resource");
    }

    size_t irq_size,reference_size;
    unsigned char *irq=(unsigned char*)read_file(join(output,"picture-irq.vram"),&irq_size);
    unsigned char *reference=(unsigned char*)read_file(join(output,"picture-reference.vram"),&reference_size);
    REQUIRE(irq_size==51840&&reference_size==51840,"Picture VRAM capture has the wrong size");
    if(memcmp(irq,reference,51840)) {
        size_t different=0,first=51840;
        for(size_t i=0;i<51840;i++) if(irq[i]!=reference[i]) { if(first==51840) first=i; different++; }
        fail("IRQ-enabled picture differs from IRQ-disabled reference at VRAM $%05zx (%zu bytes)",first,different);
    }
    size_t bitmap_nondefault=0,priority_nondefault=0;
    for(size_t i=0;i<38400;i++) if(reference[i]!=0xff) bitmap_nondefault++;
    for(size_t i=38400;i<51840;i++) if(reference[i]!=0x44) priority_nondefault++;
    REQUIRE(bitmap_nondefault&&priority_nondefault,"Picture or priority plane contains no rendered output");
    fprintf(stderr,"Production picture test: pic=1 load.pic+draw.pic and draw.pic; command entries=%llu/%llu then %llu/%llu; IRQ draw cycles=%llu asserted=%llu serviced=%llu busy IRQs=%llu max IRQ=%llu; IRQ-disabled reference missed=%llu VSYNC(s); fill calls=%llu/%llu, priority splits=%llu/%llu; VRAM 51840 bytes identical (bitmap nondefault=%zu, priority nondefault=%zu)\n",
        (unsigned long long)rows[0][13],(unsigned long long)rows[0][14],(unsigned long long)rows[1][13],(unsigned long long)rows[1][14],
        (unsigned long long)rows[0][2],(unsigned long long)rows[0][3],(unsigned long long)rows[0][4],
        (unsigned long long)rows[0][6],(unsigned long long)rows[0][7],(unsigned long long)rows[1][5],
        (unsigned long long)rows[0][9],(unsigned long long)rows[1][9],(unsigned long long)rows[0][10],
        (unsigned long long)rows[1][10],bitmap_nondefault,priority_nondefault);
    fprintf(stderr,"Production agiBlit failures: real load.view/set.view/draw handlers ran; bESetLoop and C allocation both returned false; nested owner depth 1->2->1->0; RAM/ROM banks and I flag preserved\n");
    free(irq);free(reference);free(rows);free(blit);
}
static void validate(const char *output,bool foreground_test) {
    size_t n,ncmd;uint64_t (*rows)[23]=csv(join(output,"irq.csv"),23,&n),(*commands)[23]=csv(join(output,"commands.csv"),11,&ncmd);
    REQUIRE(ncmd==5,"Missing commands");const int opcode[]={98,99,110,110,110},operand[]={0,0,1,0,2},vsync[]={0,0,33,0,65};
    for(size_t i=0;i<5;i++) { REQUIRE(commands[i][0]==(unsigned)opcode[i]&&commands[i][1]==(unsigned)operand[i],"Unexpected injected command");REQUIRE(commands[i][4]==(unsigned)vsync[i],"Incorrect command VSYNC count");REQUIRE(commands[i][5]&&commands[i][6]&&commands[i][7],"Restoration witness missing"); }
    size_t active=0,writes=0;uint64_t maxirq=0,maxshake=0,maxwrite=0,calls=0;bool sound=false,sprite=false,input=false;
    for(size_t i=0;i<n;i++) { uint64_t *r=rows[i];if(r[1]>maxirq)maxirq=r[1];if(!r[5])continue;active++;if(r[1]>maxshake)maxshake=r[1];calls|=r[8];sound|=r[7]&&r[13];sprite|=r[14]!=0;input|=r[15]!=0;if(r[10]) { writes++;if(r[12]>maxwrite)maxwrite=r[12]; } }
    REQUIRE(active&&writes,"No full-game shake activity");REQUIRE(maxwrite<11520,"Window write missed VGA blanking");REQUIRE(sound,"No actual playing PSG channels during shake");REQUIRE(sprite,"No actual sprite attribute writes during shake");REQUIRE(input,"No nonempty input-line redraw during shake");REQUIRE(commands[2][10]>0,"Keyboard was not queued while foreground waited");REQUIRE((calls&63)==63,"Required IRQ paths not observed");
    if(foreground_test) {
        size_t foreground_rows;uint64_t (*foreground)[23]=csv(join(output,"foreground.csv"),8,&foreground_rows);
        size_t picture_calls=0,object_calls=0,picture_vsyncs=0,object_vsyncs=0;uint64_t max_cycles=0,max_busy_irq=0,busy_irqs=0,object_busy_irqs=0;
        for(size_t i=0;i<foreground_rows;i++) {
            if(foreground[i][0]==1) { picture_calls++;picture_vsyncs+=foreground[i][2]; }
            else if(foreground[i][0]==2) { object_calls++; if(foreground[i][2]) { object_vsyncs+=foreground[i][2]; object_busy_irqs+=foreground[i][5]; } }
            REQUIRE(foreground[i][4]==0,"Foreground caller lost a production VSYNC");
            if(foreground[i][1]>max_cycles)max_cycles=foreground[i][1];
            busy_irqs+=foreground[i][5];if(foreground[i][6]>max_busy_irq)max_busy_irq=foreground[i][6];
            REQUIRE(foreground[i][7]==0,"Foreground caller entered with IRQs masked");
        }
        REQUIRE(object_calls&&object_vsyncs&&object_busy_irqs,"No actual b9UpdateObjects workload crossed a VSYNC with a busy IRQ");
        REQUIRE(max_busy_irq<4096,"Foreground IRQ exceeded 4096 CPU cycles");
        fprintf(stderr,"Production renderer timing: b11DrawPic calls=%zu, b9UpdateObjects calls=%zu, picture VSYNCs=%zu, max caller cycles=%llu, busy IRQs=%llu, max busy IRQ cycles=%llu\n",picture_calls,object_calls,picture_vsyncs,(unsigned long long)max_cycles,(unsigned long long)busy_irqs,(unsigned long long)max_busy_irq);
        free(foreground);
    }
    uint64_t original[4]={0};
    for(size_t c=2;c<5;c++) {
        unsigned count=(unsigned)commands[c][1];REQUIRE(commands[c][8]==count*32,"Incorrect command window writes");if(!count)continue;
        size_t last=0,total=0;for(size_t i=0;i<n;i++)if(rows[i][0]>commands[c][2]&&rows[i][0]<commands[c][3]) {last=i;total++;}
        REQUIRE(total==count*32+1,"Incorrect phase sample count");for(int k=0;k<4;k++)original[k]=rows[last][16+k];size_t index=0;
        for(size_t i=0;i<n;i++)if(rows[i][0]>commands[c][2]&&rows[i][0]<commands[c][3]) {
            uint64_t *r=rows[i];bool ended=index==count*32;REQUIRE(r[21]==(ended?0:8-(index/4)%8)&&r[22]==(ended?0:4-index%4),"Four-frame phase cadence mismatch");
            for(int k=0;k<4;k++) { uint64_t expected=original[k]+(!ended&&(index/4)%2==0&&original[(k/2)*2+1]<=251?4:0);REQUIRE(r[16+k]==expected,"Display window phase mismatch"); }
            REQUIRE(r[20]==(ended?rows[last][20]:0),"Border phase mismatch");index++;
        }
    }
    char *gif=join(output,"game.gif");size_t gifsize;unsigned char *header=(unsigned char*)read_file(gif,&gifsize);
    REQUIRE(gifsize>=10&&(!memcmp(header,"GIF87a",6)||!memcmp(header,"GIF89a",6)),"Invalid GIF capture");unsigned width=header[6]|header[7]<<8,height=header[8]|header[9]<<8;free(header);
    REQUIRE(width>=608&&height>=368,"Capture too small for visual checks");char *raw=join(output,"frames.rgb"),*rawarg=format("rgb:%s",raw);
    char *convert[]={"magick",gif,"-coalesce","-alpha","off","-depth","8",rawarg,NULL};run_from(convert,join(output,"visual.log"),90,NULL);
    size_t bytes;unsigned char *images=(unsigned char*)read_file(raw,&bytes);size_t framebytes=(size_t)width*height*3;REQUIRE(bytes%framebytes==0,"Truncated RGB frames");size_t frames=bytes/framebytes;REQUIRE(frames>100,"Missing full-game visual capture");
    int vertical=original[2]==0?7:8;size_t pair=0;
    for(size_t i=16;i<48&&i+4<frames;i++) { unsigned char *shifted=images+i*framebytes,*normal=images+(i+4)*framebytes;
        if(crop_equal(normal,shifted,width,32,128,592,360,16,vertical)&&crop_equal(normal,shifted,width,16,0,590,72,16,vertical)&&!crop_equal(normal,shifted,width,32,128,592,360,0,0)) {pair=i;break;} }
    REQUIRE(pair,"No coherent bitmap/sprite/L1 movement in real game frames");REQUIRE(crop_equal(images+(pair+4)*framebytes,images+(frames-1)*framebytes,width,32,128,592,360,0,0),"Game picture did not visually restore");
    const char *pngnames[]={"before.png","shifted.png","original_phase.png","restored.png"};size_t indices[]={2,pair,pair+4,frames-1};for(int i=0;i<4;i++)save_png(join(output,pngnames[i]),images+indices[i]*framebytes,width,height);
    char *log=read_file(join(output,"run.log"),NULL);REQUIRE(strstr(log,"Keyboard processed after resume: 1"),"Queued key not processed after resume");free(log);
    char *reportpath=join(output,"report.json");FILE *report=fopen(reportpath,"w");REQUIRE(report,"Cannot write report");char *q=quoted(output);
    fprintf(report,"{\n  \"output\": %s,\n  \"irq_samples\": %zu,\n  \"active_shake_samples\": %zu,\n  \"irq_cycles_max\": %llu,\n  \"shake_irq_cycles_max\": %llu,\n  \"vsync_to_window_write_cycles_max\": %llu,\n  \"active_calls_mask\": %llu,\n  \"commands\": [\n",q,n,active,(unsigned long long)maxirq,(unsigned long long)maxshake,(unsigned long long)maxwrite,(unsigned long long)calls);free(q);
    const char *keys[]={"opcode","operand","entry_cycle","exit_cycle","vsyncs","display_restored","code_window_restored","cursor_restored","window_writes","actual_command_entered","kbd_queue"};
    for(int i=0;i<5;i++) {fprintf(report,"    {");for(int k=0;k<11;k++)fprintf(report,"%s\"%s\": \"%llu\"",k?", ":"",keys[k],(unsigned long long)commands[i][k]);fprintf(report,"}%s\n",i==4?"":",");}
    fprintf(report,"  ],\n  \"gif_frames\": %zu,\n  \"visual_pair\": [%zu, %zu],\n  \"visual_physical_displacement\": [16, %d],\n  \"keyboard_queued_during_shake\": true,\n  \"keyboard_processed_after_resume\": true,\n  \"limits\": \"Finite KQ1 sample with forced dirty menu/score, replayed game sprite batch, and temporary LOOK input fixture. Not all-game worst case or physical hardware. This emulator renders the 0->4 VSTART change as 7 pixels due to its eff_y_fp initialization; registers retain correct hardware units.\"\n}\n",frames,pair,pair+4,vertical);REQUIRE(!fclose(report),"Report close failed");char *result=read_file(reportpath,NULL);puts(result);free(result);free(images);free(rows);free(commands);
}
int main(int argc,char **argv) {
    char *emulator=NULL,*rom=NULL,*build=NULL,*game=NULL,*output=NULL,*sdl=NULL,*symbol=NULL;unsigned long trigger=180000000;bool foreground_test=false,picture_test=false;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--help")) {puts("Usage: test_full_game_irq --emulator-source DIR --rom FILE --build DIR --game DIR [--output DIR] [--symbols FILE] [--sdl-prefix DIR] [--trigger-cycle N] [--foreground-test] [--picture-test]");return 0;}
        if(!strcmp(argv[i],"--foreground-test")) {foreground_test=true;continue;}
        if(!strcmp(argv[i],"--picture-test")) {picture_test=true;continue;}
        REQUIRE(i+1<argc,"Missing option argument");char *option=argv[i],*value=argv[++i];
        if(!strcmp(option,"--emulator-source"))emulator=value;else if(!strcmp(option,"--rom"))rom=value;else if(!strcmp(option,"--build"))build=value;else if(!strcmp(option,"--game"))game=value;else if(!strcmp(option,"--output"))output=value;else if(!strcmp(option,"--sdl-prefix"))sdl=value;else if(!strcmp(option,"--symbols"))symbol=value;
        else if(!strcmp(option,"--trigger-cycle")) {char *end;errno=0;trigger=strtoul(value,&end,10);REQUIRE(!errno&&*value&&!*end&&trigger<=UINT32_MAX,"Invalid trigger cycle");}else fail("Unknown option %s",option);
    }
    REQUIRE(emulator&&rom&&build&&game,"Require --emulator-source, --rom, --build and --game");
    if(sdl) REQUIRE(exists(sdl),"SDL prefix does not exist");
    emulator=realpath(emulator,NULL);rom=realpath(rom,NULL);build=realpath(build,NULL);game=realpath(game,NULL);REQUIRE(emulator&&rom&&build&&game,"Input path does not exist");
    if(!output) {const char *tmp=getenv("TMPDIR");output=format("%s/full-game-irq-XXXXXX",tmp?tmp:"/tmp");REQUIRE(mkdtemp(output),"Cannot create temporary output");}else mkdirs(output);
    output=realpath(output,NULL);REQUIRE(output,"Cannot resolve output");
    size_t source_len=strlen(emulator);
    REQUIRE(strncmp(output,emulator,source_len)||(output[source_len]&&output[source_len]!='/'),"Output must be outside emulator source tree");
    /* A fresh output prevents stale logs or previously patched emulator sources. */
    REQUIRE(!exists(join(output,"emulator")),"Output already contains an emulator; use a fresh directory");
    char *source=join(output,"emulator"),*stage=join(output,"stage");copy_tree(emulator,source);mkdirs(stage);copy_stage(game,stage,false);copy_stage(build,stage,true);
    const char *candidates[]={"final.lbl","agi.cx16.lbl","final.dbg","agi.cx16.dbg"};
    if(!symbol)for(size_t i=0;i<4;i++) {char *p=join(build,candidates[i]);if(exists(p)){symbol=p;break;}free(p);}
    REQUIRE(symbol,"Provide --symbols with private assembly symbols");load_symbols(join(build,"final.lbl"));load_symbols(join(build,"agi.cx16.lbl"));load_symbols(symbol);
    char *main_c=join(source,"src/main.c"),*cpu=join(source,"src/cpu/fake6502.c"),*video=join(source,"src/video.c"),*i2c=join(source,"src/i2c.c");
    char *injected=NULL;size_t size=0;FILE *mem=open_memstream(&injected,&size);REQUIRE(mem,"Out of memory");
    char *qcsv=quoted_c(join(output,"irq.csv")),*qcommands=quoted_c(join(output,"commands.csv")),*qforeground=quoted_c(join(output,"foreground.csv"));
    char *qpicture=quoted_c(join(output,"picture.csv")),*qpicture_irq=quoted_c(join(output,"picture-irq.vram")),*qpicture_reference=quoted_c(join(output,"picture-reference.vram")),*qblit=quoted_c(join(output,"agi-blit-failures.csv"));
    fprintf(mem,"#define SHAKE_CSV %s\n#define SHAKE_COMMANDS %s\n#define FOREGROUND_CSV %s\n#define FOREGROUND_TEST_ENABLED %u\n#define PICTURE_CSV %s\n#define PICTURE_IRQ_VRAM %s\n#define PICTURE_REFERENCE_VRAM %s\n#define BLIT_FAILURE_CSV %s\n#define PICTURE_TEST_ENABLED %u\n#define TRIGGER_CYCLE %luu\n",qcsv,qcommands,qforeground,(foreground_test||picture_test)?1:0,qpicture,qpicture_irq,qpicture_reference,qblit,picture_test?1:0,trigger);
    free(qcsv);free(qcommands);free(qforeground);free(qpicture);free(qpicture_irq);free(qpicture_reference);free(qblit);
    for(size_t i=0;i<NAME_COUNT;i++) {if(!found[i])fail("Missing symbol %s",names[i]);if(names[i][0]=='@')fprintf(mem,"#define S_local_%s 0x%x\n",names[i]+1,labels[i]);else fprintf(mem,"#define S_%s 0x%x\n",names[i],labels[i]);}
    fputs(hook,mem);fputs("\nvoid *emulator_loop(void *param);",mem);REQUIRE(!fclose(mem),"Hook buffer close failed");patch(main_c,"void *emulator_loop(void *param);",injected,1);free(injected);
    patch(main_c,"\t\tstep6502();","\t\tshake_before_step();\n\t\tuint8_t shake_opcode=waiting ? 0 : debug_read6502(regs.pc,regs.k,USE_CURRENT_X16_BANK);\n\t\tstep6502();\n\t\tshake_after_step(shake_opcode);",1);
    patch(cpu,"void irq6502() {","extern void shake_irq_accepted(void);\nextern int shake_suppress_irq(void);\nvoid irq6502() {",1);
    patch(cpu,"        interrupt6502(INT_IRQ);","        if(!shake_suppress_irq()) { shake_irq_accepted(); interrupt6502(INT_IRQ); }",1);
    patch(video,"void video_write(uint8_t reg, uint8_t value) {","void shake_get_display(uint8_t *values) { for(int i=0;i<4;i++) values[i]=reg_composer[4+i]; values[4]=reg_composer[3]; }\nextern void shake_window_write(void);\nextern void shake_foreground_vera_register_write(uint8_t,uint8_t);\nvoid video_write(uint8_t reg, uint8_t value) {\n\tif(io_dcsel==0 && (reg<=2 || reg==5)) shake_foreground_vera_register_write(reg,io_dcsel);\n\tif(io_dcsel==1 && reg>=9 && reg<=12) shake_window_write();",1);
    patch(video,"\t\tisr |= 1; // VSYNC IRQ","\t\textern void shake_vsync_asserted(void);\n\t\tshake_vsync_asserted();\n\t\tisr |= 1; // VSYNC IRQ",1);
    patch(video,"video_space_write(uint32_t address, uint8_t value)\n{","video_space_write(uint32_t address, uint8_t value)\n{\n\textern void shake_sprite_write(void);\n\textern void shake_foreground_vram_write(uint32_t);\n\tshake_foreground_vram_write(address);\n\tif(address>=0x1fc00 && address<0x20000) shake_sprite_write();",1);
    patch(video,"fx_video_space_write(uint32_t address, bool nibble, uint8_t value)\n{","fx_video_space_write(uint32_t address, bool nibble, uint8_t value)\n{\n\textern void shake_sprite_write(void);\n\textern void shake_foreground_vram_write(uint32_t);\n\tshake_foreground_vram_write(address);\n\tif(address>=0x1fc00 && address<0x20000) shake_sprite_write();",1);
    patch(video,"psg_writereg(address & 0x3f, value);","extern void shake_psg_write(uint8_t reg,uint8_t value);\n\t\tshake_psg_write(address & 0x3f,value);\n\t\tpsg_writereg(address & 0x3f, value);",2);
    patch(i2c,"\nvoid i2c_reset_state() {","\nvoid shake_get_i2c_state(uint8_t *out) { out[0]=i2c_port.clk_in; out[1]=i2c_port.data_in; out[2]=i2c_port.data_out; out[3]=(uint8_t)(state+1); out[4]=read_mode; out[5]=value; out[6]=(uint8_t)count; out[7]=device; }\n\nvoid i2c_reset_state() {",1);
    patch(join(source,"src/disasm.c"),"char *where = strstr(mnemonic, \"%%0%hhux\");","const char *where = strstr(mnemonic, \"%%0%hhux\");",1);
    char *native=join(output,"native"),*buildlog=join(output,"build.log");
    if(!sdl) {char *parent=strdup(emulator),*slash=strrchr(parent,'/');REQUIRE(slash,"Invalid emulator path");*slash=0;sdl=join(parent,"sdl2-install");free(parent);}
    char *prefix=NULL;if(exists(sdl)) {char *absolute=realpath(sdl,NULL);REQUIRE(absolute,"Cannot resolve SDL prefix");prefix=format("-DCMAKE_PREFIX_PATH=%s",absolute);free(absolute);}
    char *configure[]={"cmake","-S",source,"-B",native,"-DENABLE_LTO=OFF","-DENABLE_TRACE=OFF","-DENABLE_FLUIDSYNTH=OFF",prefix,NULL};run_from(configure,buildlog,0,NULL);
    char *compile[]={"cmake","--build",native,"-j4",NULL};run_from(compile,buildlog,0,NULL);
    REQUIRE(!setenv("SDL_VIDEODRIVER","dummy",1)&&!setenv("SDL_AUDIODRIVER","dummy",1),"Cannot set SDL environment");
    char *tas=join(output,"input.tas");write_file(tas,"0:Return\n",9);
    char *command[24];int command_length=0;
    command[command_length++]=join(native,"x16emu");command[command_length++]="-rom";command[command_length++]=rom;
    command[command_length++]="-fsroot";command[command_length++]=stage;command[command_length++]="-startin";command[command_length++]=stage;
    command[command_length++]="-prg";command[command_length++]=join(stage,"agi.cx16");command[command_length++]="-run";command[command_length++]="-zeroram";
    command[command_length++]="-warp";command[command_length++]="-sound";command[command_length++]="none";
    if(!picture_test) { command[command_length++]="-gif";command[command_length++]=format("%s/game.gif,wait",output); }
    command[command_length++]="-tas";command[command_length++]=tas;command[command_length++]="-echo";command[command_length]=NULL;
    run_from(command,join(output,"run.log"),picture_test?300:90,NULL);
    if(picture_test) validate_picture(output); else validate(output,foreground_test);return 0;
}
