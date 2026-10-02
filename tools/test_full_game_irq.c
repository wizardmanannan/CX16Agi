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
/* Use argv directly: paths and game names never become shell commands. */
static void run(char *const argv[],const char *log,int timeout) {
    pid_t pid=fork(); if(pid<0) fail("fork: %s",strerror(errno));
    if(!pid) { if(log) { FILE *f=fopen(log,"a"); if(!f) _exit(126); dup2(fileno(f),1);dup2(fileno(f),2);fclose(f); } execvp(argv[0],argv); perror(argv[0]);_exit(127); }
    int status; struct timespec start,now;
    REQUIRE(!clock_gettime(CLOCK_MONOTONIC,&start),"Cannot read monotonic clock");
    for(;;) { pid_t r=waitpid(pid,&status,WNOHANG); if(r==pid) break; if(r<0&&errno!=EINTR) fail("waitpid: %s",strerror(errno));
        REQUIRE(!clock_gettime(CLOCK_MONOTONIC,&now),"Cannot read monotonic clock");
        if(timeout&&(now.tv_sec-start.tv_sec>timeout||(now.tv_sec-start.tv_sec==timeout&&now.tv_nsec>=start.tv_nsec))) { kill(pid,SIGKILL);waitpid(pid,&status,0);fail("%s timed out (%d seconds); see %s",argv[0],timeout,log?log:"output"); }
        struct timespec delay={0,100000000};nanosleep(&delay,NULL);
    }
    if(!WIFEXITED(status)||WEXITSTATUS(status)) fail("%s failed (status %d); see %s",argv[0],status,log?log:"output");
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
"extern void shake_get_display(uint8_t *values);\n"
"static const uint8_t shake_commands_data[][3] = {{98,0,0},{99,0,240},{110,1,0},{110,0,0},{110,2,0}};\n"
"static const int shake_command_lengths[] = {2,3,2,2,2};\n"
"static uint8_t shake_read(uint16_t address) { return debug_read6502(address,0,USE_CURRENT_X16_BANK); }\n"
"static void shake_ram(uint16_t address,uint8_t value) { write6502(address,0,value); }\n"
"void shake_irq_accepted(void) {\n"
"    if(!shake_irq_active) { shake_accept=clockticks6502; shake_return_pc=regs.pc; shake_return_sp=regs.sp;\n"
"        shake_return_a=regs.a; shake_return_x=regs.xl; shake_return_y=regs.yl;\n"
"        shake_return_status=regs.status & 0xcf; // C/Z/I/D/V/N; exclude synthetic B/U.\n"
"        shake_return_bank=memory_get_ram_bank(); shake_return_rom=memory_get_rom_bank(); }\n"
"}\n"
"void shake_vsync_asserted(void) { shake_vsync=clockticks6502; shake_frames_seen++; }\n"
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
"static void shake_before_step(void) {\n"
"    if(!shake_csv) {\n"
"        shake_csv=fopen(SHAKE_CSV,\"w\"); shake_commands=fopen(SHAKE_COMMANDS,\"w\");\n"
"        if(!shake_csv || !shake_commands) { perror(\"profile output\"); exit(2); }\n"
"        fprintf(shake_csv,\"cycle,irq_cycles,custom_cycles,accept_delay,custom_delay,count,phase,channels,calls,psg_writes,window_writes,first_write,last_write,audible_writes,sprite_writes,input_chars,hstart,hstop,vstart,vstop,border,phase_after,frames_after\\n\");\n"
"        fprintf(shake_commands,\"opcode,operand,entry_cycle,exit_cycle,vsyncs,display_restored,code_window_restored,cursor_restored,window_writes,actual_command_entered,kbd_queue\\n\");\n"
"    }\n"
"    if(regs.k || waiting) return;\n"
"    uint16_t pc=regs.pc; uint8_t bank=memory_get_ram_bank();\n"
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
static const char *names[] = {"custom_irq_handler","mainLoop","codeWindow","cwCurrentCode","b4ShakeScreen","shakeCount","shakePhases","shakeFrames","_menuDirty","_menuShown","_mainLoopCounter","_bBChannelsPlaying","bBSoundHandler","b7HandleInputLine","bADisplayMenu","b10DisplayStatusBar","bEHandleSpriteUpdates","b10PrintStatusBarString","handleScreenShake","@handleSound","_inputLineDisplayed","_b7CurrentInputStr","_bESpritesUpdatedBuffer","_bESpritesUpdatedBufferPointer"};
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
static void validate(const char *output) {
    size_t n,ncmd;uint64_t (*rows)[23]=csv(join(output,"irq.csv"),23,&n),(*commands)[23]=csv(join(output,"commands.csv"),11,&ncmd);
    REQUIRE(ncmd==5,"Missing commands");const int opcode[]={98,99,110,110,110},operand[]={0,0,1,0,2},vsync[]={0,0,33,0,65};
    for(size_t i=0;i<5;i++) { REQUIRE(commands[i][0]==(unsigned)opcode[i]&&commands[i][1]==(unsigned)operand[i],"Unexpected injected command");REQUIRE(commands[i][4]==(unsigned)vsync[i],"Incorrect command VSYNC count");REQUIRE(commands[i][5]&&commands[i][6]&&commands[i][7],"Restoration witness missing"); }
    size_t active=0,writes=0;uint64_t maxirq=0,maxshake=0,maxwrite=0,calls=0;bool sound=false,sprite=false,input=false;
    for(size_t i=0;i<n;i++) { uint64_t *r=rows[i];if(r[1]>maxirq)maxirq=r[1];if(!r[5])continue;active++;if(r[1]>maxshake)maxshake=r[1];calls|=r[8];sound|=r[7]&&r[13];sprite|=r[14]!=0;input|=r[15]!=0;if(r[10]) { writes++;if(r[12]>maxwrite)maxwrite=r[12]; } }
    REQUIRE(active&&writes,"No full-game shake activity");REQUIRE(maxwrite<11520,"Window write missed VGA blanking");REQUIRE(sound,"No actual playing PSG channels during shake");REQUIRE(sprite,"No actual sprite attribute writes during shake");REQUIRE(input,"No nonempty input-line redraw during shake");REQUIRE(commands[2][10]>0,"Keyboard was not queued while foreground waited");REQUIRE((calls&63)==63,"Required IRQ paths not observed");
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
    char *convert[]={"magick",gif,"-coalesce","-alpha","off","-depth","8",rawarg,NULL};run(convert,join(output,"visual.log"),90);
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
    char *emulator=NULL,*rom=NULL,*build=NULL,*game=NULL,*output=NULL,*sdl=NULL,*symbol=NULL;unsigned long trigger=180000000;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--help")) {puts("Usage: test_full_game_irq --emulator-source DIR --rom FILE --build DIR --game DIR [--output DIR] [--symbols FILE] [--sdl-prefix DIR] [--trigger-cycle N]");return 0;}
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
    char *main_c=join(source,"src/main.c"),*cpu=join(source,"src/cpu/fake6502.c"),*video=join(source,"src/video.c");
    char *injected=NULL;size_t size=0;FILE *mem=open_memstream(&injected,&size);REQUIRE(mem,"Out of memory");
    char *qcsv=quoted_c(join(output,"irq.csv")),*qcommands=quoted_c(join(output,"commands.csv"));fprintf(mem,"#define SHAKE_CSV %s\n#define SHAKE_COMMANDS %s\n#define TRIGGER_CYCLE %luu\n",qcsv,qcommands,trigger);free(qcsv);free(qcommands);
    for(size_t i=0;i<NAME_COUNT;i++) {if(!found[i])fail("Missing symbol %s",names[i]);if(names[i][0]=='@')fprintf(mem,"#define S_local_%s 0x%x\n",names[i]+1,labels[i]);else fprintf(mem,"#define S_%s 0x%x\n",names[i],labels[i]);}
    fputs(hook,mem);fputs("\nvoid *emulator_loop(void *param);",mem);REQUIRE(!fclose(mem),"Hook buffer close failed");patch(main_c,"void *emulator_loop(void *param);",injected,1);free(injected);
    patch(main_c,"\t\tstep6502();","\t\tshake_before_step();\n\t\tuint8_t shake_opcode=waiting ? 0 : debug_read6502(regs.pc,regs.k,USE_CURRENT_X16_BANK);\n\t\tstep6502();\n\t\tshake_after_step(shake_opcode);",1);
    patch(cpu,"void irq6502() {","extern void shake_irq_accepted(void);\nvoid irq6502() {",1);
    patch(cpu,"        interrupt6502(INT_IRQ);","        shake_irq_accepted();\n        interrupt6502(INT_IRQ);",1);
    patch(video,"void video_write(uint8_t reg, uint8_t value) {","void shake_get_display(uint8_t *values) { for(int i=0;i<4;i++) values[i]=reg_composer[4+i]; values[4]=reg_composer[3]; }\nextern void shake_window_write(void);\nvoid video_write(uint8_t reg, uint8_t value) {\n\tif(io_dcsel==1 && reg>=9 && reg<=12) shake_window_write();",1);
    patch(video,"\t\tisr |= 1; // VSYNC IRQ","\t\textern void shake_vsync_asserted(void);\n\t\tshake_vsync_asserted();\n\t\tisr |= 1; // VSYNC IRQ",1);
    patch(video,"video_space_write(uint32_t address, uint8_t value)\n{","video_space_write(uint32_t address, uint8_t value)\n{\n\textern void shake_sprite_write(void);\n\tif(address>=0x1fc00 && address<0x20000) shake_sprite_write();",1);
    patch(video,"fx_video_space_write(uint32_t address, bool nibble, uint8_t value)\n{","fx_video_space_write(uint32_t address, bool nibble, uint8_t value)\n{\n\textern void shake_sprite_write(void);\n\tif(address>=0x1fc00 && address<0x20000) shake_sprite_write();",1);
    patch(video,"psg_writereg(address & 0x3f, value);","extern void shake_psg_write(uint8_t reg,uint8_t value);\n\t\tshake_psg_write(address & 0x3f,value);\n\t\tpsg_writereg(address & 0x3f, value);",2);
    char *native=join(output,"native"),*buildlog=join(output,"build.log");
    if(!sdl) {char *parent=strdup(emulator),*slash=strrchr(parent,'/');REQUIRE(slash,"Invalid emulator path");*slash=0;sdl=join(parent,"sdl2-install");free(parent);}
    char *prefix=NULL;if(exists(sdl)) {char *absolute=realpath(sdl,NULL);REQUIRE(absolute,"Cannot resolve SDL prefix");prefix=format("-DCMAKE_PREFIX_PATH=%s",absolute);free(absolute);}
    char *configure[]={"cmake","-S",source,"-B",native,"-DENABLE_LTO=OFF","-DENABLE_TRACE=OFF","-DENABLE_FLUIDSYNTH=OFF",prefix,NULL};run(configure,buildlog,0);
    char *compile[]={"cmake","--build",native,"-j4",NULL};run(compile,buildlog,0);
    REQUIRE(!setenv("SDL_VIDEODRIVER","dummy",1)&&!setenv("SDL_AUDIODRIVER","dummy",1),"Cannot set SDL environment");
    char *tas=join(output,"input.tas");write_file(tas,"0:Return\n",9);
    char *command[]={join(native,"x16emu"),"-rom",rom,"-fsroot",stage,"-startin",stage,"-prg",join(stage,"agi.cx16"),"-run","-zeroram","-warp","-sound","none","-gif",format("%s/game.gif,wait",output),"-tas",tas,"-echo",NULL};run(command,join(output,"run.log"),90);
    validate(output);return 0;
}
