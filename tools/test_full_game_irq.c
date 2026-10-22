#define _GNU_SOURCE
#include "test_paths.h"
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
#include "../src/spriteUpdates.h"
#include "test_words_fixture.h"
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
static void require_no_retained_stage(const char *output) {
    char *stage=join(output,"stage");
    if(exists(stage)) fail("integration fixture cleanup regression: staged game remains in retained output: %s",stage);
    free(stage);
}
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
static void copy_stage(const char *from,const char *to,bool build,bool omit_object) {
    DIR *d=opendir(from); if(!d) fail("opendir %s",from); struct dirent *e;
    while((e=readdir(d))) { bool agi=!strncasecmp(e->d_name,"agi.cx16",8); if(agi!=build||(omit_object&&!strcasecmp(e->d_name,"object"))) continue;
        char *a=join(from,e->d_name),*b=join(to,e->d_name); struct stat st;
        if(!stat(a,&st)) {
            if(S_ISREG(st.st_mode)) copy_file(a,b);
            else if(!build&&S_ISDIR(st.st_mode)&&!strcasecmp(e->d_name,"meta")) copy_tree(a,join(to,"meta"));
        }
        free(a);free(b);
    } closedir(d);
    /* The shared game cache keeps extractor output beside the original files. */
    char *extracted=join(from,"extracted"),*metadata=join(to,"meta");
    if(!build&&!exists(metadata)&&(d=opendir(extracted))) {
        while((e=readdir(d))) {
            char *lower=strdup(e->d_name); REQUIRE(lower,"Out of memory");
            for(char *p=lower;*p;++p) *p=(char)tolower((unsigned char)*p);
            if(strstr(lower,"-ids.idx")||strstr(lower,"-ids.bin")) {
                mkdirs(metadata); copy_file(join(extracted,e->d_name),join(metadata,lower));
            }
            free(lower);
        }
        closedir(d);
    }
    free(extracted); free(metadata);
}
static void run(char *const argv[],const char *log,int timeout) {
    run_from(argv,log,timeout,NULL);
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
static bool file_contains(const char *path,const char *needle) {
    if(!exists(path)) return false;
    char *contents=read_file(path,NULL); bool found=strstr(contents,needle)!=NULL; free(contents); return found;
}
static const char hook[] =
"\n"
"static FILE *shake_csv, *shake_commands;\n"
"static FILE *shake_input_trace;\n"
"static FILE *shake_video_trace; static uint32_t shake_video_frame_index;\n"
"static uint32_t shake_real_b9_entries;\n"
"static uint32_t shake_vsync, shake_accept, shake_custom, shake_first_write, shake_last_write;\n"
"static uint32_t shake_entry_vsync, shake_calls, shake_psg_writes, shake_window_writes, shake_audible_writes, shake_sprite_writes;\n"
"static uint8_t shake_input_chars, shake_saved_input[40];\n"
"static bool shake_keyboard_queued, shake_input_replaced;\n"
"static uint32_t shake_frames_seen, shake_start_frames;\n"
"static uint8_t shake_entry_count, shake_entry_phase, shake_entry_channels;\n"
"static bool shake_irq_active, shake_injected;\n"
"static int shake_command_index;\n"
"static uint8_t shake_saved_window[50], shake_saved_cursor[2], shake_saved_pointer[2], shake_saved_end[2], shake_saved_code_window_invalid;\n"
"static uint32_t shake_command_start, shake_capture_end, shake_total_writes, shake_command_writes;\n"
"static uint16_t shake_return_pc, shake_return_sp;\n"
"static uint8_t shake_return_a,shake_return_x,shake_return_y,shake_return_bank,shake_return_rom;\n"
"static uint8_t shake_return_status;\n"
"static uint8_t shake_saved_display[5], shake_helper_ctrl;\n"
"static bool shake_command_entered, shake_command_cursor_seen, shake_command_handler_seen, shake_helper_entered, shake_write_pending, shake_capture_started;\n"
"static bool shake_view_warm_started, shake_view_warm_active, shake_view_warm_script_complete, shake_view_warm_finished;\n"
"static unsigned shake_view_warm_load_entries, shake_view_warm_animate_entries, shake_view_warm_set_entries, shake_view_warm_position_entries, shake_view_warm_draw_entries, shake_view_warm_b9_entries;\n"
"static unsigned shake_view_warm_total_b9_entries, shake_view_warm_b9_since_handler, shake_view_warm_last_batch_b9_entries, shake_view_warm_sprite_handler_entries;\n"
"static unsigned shake_view_warm_outer_owner_b9_entries;\n"
"static uint8_t shake_view_warm_script_length, shake_view_warm_first_script_length;\n"
"static bool shake_view_warm_stale_entry_seen, shake_view_warm_second_script_injected, shake_view_warm_second_entry_pending;\n"
"static bool shake_view_warm_second_batch_consumed, shake_view_warm_first_snapshot_saved, shake_view_warm_latest_snapshot_saved;\n"
"static bool shake_view_warm_outer_owner_active, shake_view_warm_outer_owner_begin_scheduled, shake_view_warm_outer_owner_begin_completed, shake_view_warm_outer_owner_end_scheduled, shake_view_warm_outer_owner_end_completed, shake_view_warm_outer_owner_release_pending;\n"
"static uint16_t shake_view_warm_outer_owner_release_pc; static uint8_t shake_view_warm_outer_owner_release_sp;\n"
"static bool shake_view_warm_progress_logged; static uint32_t shake_view_warm_outer_frames_start, shake_view_warm_outer_service_start; static unsigned shake_view_warm_outer_handlers_start;\n"
"static bool shake_sprite_owner_wrapper_active, shake_sprite_owner_wrapper_is_end, shake_sprite_owner_wrapper_progress_logged; static uint32_t shake_sprite_owner_wrapper_start_cycle;\n"
"static uint16_t shake_sprite_owner_wrapper_return_pc, shake_sprite_owner_wrapper_return_sp, shake_sprite_owner_wrapper_c_sp;\n"
"static uint8_t shake_sprite_owner_wrapper_ram_bank, shake_sprite_owner_wrapper_rom_bank, shake_sprite_owner_wrapper_code_bank, shake_sprite_owner_wrapper_status, shake_sprite_owner_wrapper_a, shake_sprite_owner_wrapper_x, shake_sprite_owner_wrapper_y, shake_sprite_owner_wrapper_expected_depth, shake_sprite_owner_wrapper_result_depth;\n"
"typedef struct { uint32_t entry_cycle, exit_cycle; uint16_t entry_pointer, exit_pointer, return_pc; uint8_t entry_depth, exit_depth, return_sp, complete; } WarmSpriteProducerCall;\n"
"static WarmSpriteProducerCall shake_view_warm_calls[256];\n"
"static unsigned shake_view_warm_call_count, shake_view_warm_active_call, shake_view_warm_last_batch_start;\n"
"static uint32_t shake_view_warm_start_cycle;\n"
"static uint8_t shake_saved_var10;\n"
"static uint8_t shake_saved_var0;\n"
"static uint8_t shake_saved_flag9;\n"
"static bool shake_start_return_queued, shake_start_return_seen, shake_start_poll_active;\n"
"static uint16_t shake_start_poll_return_pc; static uint8_t shake_start_poll_return_sp;\n"
"static bool startup_text_active;\n"
"static bool startup_text_negative_control_injected, startup_text_negative_control_active;\n"
"static bool startup_text_negative_control_rejected, startup_text_published_initialized;\n"
"static bool startup_text_fixed_rows_seen;\n"
"static unsigned startup_text_returns, startup_text_phase, startup_text_phase_call;\n"
"static unsigned startup_text_frame_samples, startup_text_visible_frame_samples;\n"
"static uint8_t startup_text_published_messages[13], startup_text_published_widths[13];\n"
"static uint8_t startup_text_published_glyphs[13][18];\n"
"static uint8_t startup_text_negative_control_row, startup_text_negative_control_column;\n"
"static uint8_t startup_text_negative_control_original;\n"
"static uint8_t startup_text_row, startup_text_column, startup_text_message;\n"
"static uint16_t startup_text_return_pc, startup_text_generation;\n"
"static uint8_t startup_text_return_sp, startup_text_entry_ram_bank;\n"
"static uint32_t startup_text_call_cycles, startup_text_call_frames, startup_text_phase_start_cycles, startup_text_phase_start_frames, startup_text_phase_cycles, startup_text_phase_frames, startup_text_total_cycles, startup_text_total_frames;\n"
"static uint8_t shake_saved_sprite_batch[SHAKE_SPRITE_BUFFER_SIZE];\n"
"static uint8_t shake_view_warm_first_snapshot[SHAKE_SPRITE_BUFFER_SIZE], shake_view_warm_latest_snapshot[SHAKE_SPRITE_BUFFER_SIZE];\n"
"static uint16_t shake_view_warm_first_snapshot_length, shake_view_warm_latest_snapshot_length;\n"
"static uint16_t shake_sprite_batch_length, shake_sprite_batch_return_pc;\n"
"static uint32_t shake_sprite_batch_handler_entries, shake_sprite_batch_empty_entries;\n"
"static uint16_t shake_sprite_batch_last_pointer;\n"
"static uint8_t shake_sprite_batch_last_first, shake_sprite_batch_last_second;\n"
"static uint8_t shake_sprite_batch_return_sp;\n"
"static bool shake_sprite_batch_saved, shake_sprite_batch_warm_latest, shake_sprite_batch_processed, shake_sprite_batch_handler_active, shake_sprite_batch_replay_active, shake_sprite_batch_replay_requested, shake_sprite_batch_replayed;\n"
"static bool shake_view_warm_coordinates_changed, shake_view_warm_irq_input_latest;\n"
"static uint8_t shake_sprite_attr_actual[0x400], shake_sprite_attr_written[0x400];\n"
"static uint32_t shake_capture_start_frames, shake_last_command_frames, shake_key_processed_frame;\n"
"static bool shake_keyboard_processed;\n"
"static FILE *foreground_csv;\n"
"static bool foreground_active, foreground_irq_active;\n"
"static int foreground_kind;\n"
"static uint16_t foreground_return_pc, foreground_irq_return_pc;\n"
"static uint8_t foreground_return_sp, foreground_irq_return_sp, foreground_entry_i, foreground_entry_depth;\n"
"static uint32_t foreground_start_cycles, foreground_start_frames, foreground_start_counter;\n"
"static uint32_t foreground_irq_start_cycles, foreground_irq_max_cycles, foreground_irq_count;\n"
"static uint8_t foreground_irq_a, foreground_irq_x, foreground_irq_y, foreground_irq_status;\n"
"static uint8_t foreground_irq_ram_bank, foreground_irq_rom_bank, foreground_irq_vera[4], foreground_irq_depth;\n"
"static uint8_t foreground_irq_zp[0x52+0x57], foreground_irq_work[500], foreground_irq_i2c[8], foreground_irq_fx[6]; static uint32_t foreground_irq_fx_accum;\n"
"static uint8_t foreground_max_irq_depth;\n"
"typedef struct { uint32_t cycles, i_masked_cycles, calls, sound_channels, psg_writes, audible_psg_writes, menu_count, menu_vram_writes, sprite_records, sprite_vram_writes, text_vram_writes; uint8_t command, work_kind, entry_depth, entry_i; } CombinedIRQRow;\n"
"static FILE *combined_csv;\n"
"static CombinedIRQRow combined_rows[2];\n"
"static bool combined_injected, combined_irq_pending, combined_irq_active, combined_complete, combined_i_contiguous, combined_i_pending_boundary_seen;\n"
"static bool combined_sound_active, combined_menu_active, combined_sprite_active, combined_text_active;\n"
"static uint16_t combined_return_pc, combined_sound_return_pc, combined_menu_return_pc, combined_sprite_return_pc, combined_text_return_pc;\n"
"static uint8_t combined_return_sp, combined_sound_return_sp, combined_menu_return_sp, combined_sprite_return_sp, combined_text_return_sp;\n"
"static uint32_t combined_accept_cycles, combined_mask_start, combined_mask_end, combined_row_start_cycles;\n"
"static uint32_t combined_row_calls, combined_sound_channels, combined_psg_writes, combined_audible_psg_writes;\n"
"static uint32_t combined_menu_vram_writes, combined_sprite_vram_writes, combined_text_vram_writes;\n"
"static uint32_t combined_row_count;\n"
"static uint8_t combined_row_command, combined_row_work_kind;\n"
"static uint32_t combined_irq_entry_count;\n"
"static uint8_t combined_irq_entry_depth, combined_irq_entry_i, combined_irq_entry_ram_bank, combined_irq_entry_code_bank;\n"
"static uint8_t combined_irq_entry_command, combined_irq_entry_work;\n"
"static uint16_t combined_irq_entry_generation;\n"
"static uint32_t combined_inject_frames, combined_inject_counter, combined_generation, combined_sprite_records, combined_menu_count;\n"
"static uint32_t combined_total_cycles, combined_max_cycles, combined_max_i_masked_cycles, combined_completion_frames, combined_asserted_frames, combined_serviced_frames;\n"
"static uint32_t combined_interpret_hits, combined_logic_hits, combined_mainloop_hits, combined_logic_entry_hits, combined_poll_hits;\n"
"static uint32_t combined_first_mainloop_cycle, combined_last_mainloop_cycle;\n"
"static bool combined_start_return_queued;\n"
"static uint16_t combined_text_buffer, combined_text_destination;\n"
"static uint32_t combined_text_total_writes, combined_text_attribute_writes;\n"
"static uint32_t combined_text_last_write_cycle, combined_generation_cycle;\n"
"static uint16_t combined_text_last_write_address; static uint8_t combined_text_last_write_value;\n"
"static uint32_t combined_text_last_attribute_write_cycle; static uint16_t combined_text_last_attribute_write_address; static uint16_t combined_text_pending_attribute_address; static uint8_t combined_text_pending_attribute_value; static bool combined_text_attribute_pending;\n"
"static bool combined_guard_trace_active, combined_guard_seed_active, combined_guard_seeded, combined_guard_menu_clear_seen;\n"
"static uint32_t combined_guard_writes, combined_guard_menu_clear_writes, combined_guard_post_seed_writes;\n"
"static uint16_t combined_guard_last_pc; static uint8_t combined_guard_last_bank, combined_guard_last_value;\n"
"static bool combined_text_publisher_active;\n"
"static uint16_t combined_text_publisher_return_pc, combined_text_publisher_return_sp, combined_text_publisher_c_sp;\n"
"static uint8_t combined_text_publisher_ram_bank, combined_text_publisher_rom_bank, combined_text_publisher_code_bank, combined_text_publisher_status;\n"
"enum { TEXT_CASE_BUFFER_END, TEXT_CASE_BANK_ALIAS, TEXT_CASE_BANK_BOUNDARY, TEXT_CASE_FULL_CLEAR, TEXT_CASE_OVERLAP, TEXT_CASE_COUNT };\n"
"static const char *const text_case_names[TEXT_CASE_COUNT]={\"owned-buffer-offset-1999-unterminated-byte\",\"bank-alias-same-address\",\"source-stops-at-c000\",\"full-1856-cell-clear\",\"overlapping-120-character-copy\"};\n"
"typedef struct { uint16_t return_pc; uint8_t return_sp; } TextSourceFrame;\n"
"static TextSourceFrame text_source_frames[8];\n"
"static unsigned text_source_depth, text_case_index;\n"
"static bool text_case_active, text_case_source_trace, text_case_vram_trace;\n"
"static bool text_case_map_seeded;\n"
"static uint16_t text_case_return_pc, text_case_return_sp, text_case_return_c_sp;\n"
"static uint8_t text_case_return_ram_bank, text_case_return_rom_bank, text_case_return_code_bank, text_case_return_status;\n"
"static uint16_t text_case_source, text_case_generation, text_case_previous_generation;\n"
"static uint8_t text_case_source_bank;\n"
"static uint32_t text_case_source_bytes, text_case_source_guard, text_case_source_reads, text_case_source_oob_reads, text_case_c000_reads;\n"
"static uint32_t text_case_char_writes, text_case_last_write_cycle, text_case_generation_cycle;\n"
"static uint16_t text_case_last_write_address; static uint8_t text_case_last_write_value;\n"
"static void text_case_vram_write(uint32_t address,uint8_t value);\n"
"static uint8_t shake_banked_read(uint16_t address,uint8_t bank);\n"
"typedef struct { uint8_t count, blocks, width, height, is_on_backbuffer, backbuffer_size, metadata_bank, view_num, loop_num, cel_num; uint16_t loop_records, backbuffer_records; uint32_t loop[256], backbuffer[256]; uint8_t loop_zero, loop_valid, loop_reserved, backbuffer_zero, backbuffer_valid, backbuffer_reserved; } BlitMetadataSnapshot;\n"
"static BlitMetadataSnapshot blit_before_set_loop, blit_after_failure, blit_after_retry, blit_before_retry;\n"
"static FILE *picture_csv;\n"
"static bool picture_irq_suppressed, picture_draw_active;\n"
"static int picture_test_stage, picture_draw_count, picture_floodfill_count, picture_split_count;\n"
"static unsigned picture_load_entries, picture_draw_entries;\n"
"static FILE *view_load_retry_csv;\n"
"static bool view_load_failure_armed, view_load_failure_active, view_load_failure_returned, view_load_loader_active;\n"
"static bool view_load_loader_returned, view_load_retry_loader_returned, view_load_view_unchanged, view_load_metadata_unchanged, view_load_allocator_unchanged;\n"
"static bool view_load_retry_observer_active, view_load_retry_blit_done;\n"
"static uint8_t view_load_target, view_load_directory[5], view_load_before_view[32], view_load_before_metadata[20][32], view_load_before_allocator[2160];\n"
"static uint8_t view_load_view_size, view_load_metadata_size, view_load_loader_result, view_load_retry_loader_result;\n"
"static uint8_t view_load_retry_blit_result, view_load_retry_loaded_view, view_load_retry_bank_preserved, view_load_retry_rom_preserved, view_load_retry_i_preserved, view_load_retry_depth_restored;\n"
"static unsigned view_load_failure_command_entries, view_load_failure_caller_entries, view_load_failure_loader_entries;\n"
"static unsigned view_load_retry_caller_entries, view_load_retry_loader_entries;\n"
"static uint16_t view_load_failure_return_pc, view_load_loader_return_pc, view_load_retry_blit_return_pc;\n"
"static uint8_t view_load_failure_return_sp, view_load_loader_return_sp, view_load_retry_blit_return_sp, view_load_retry_entry_bank, view_load_retry_entry_rom, view_load_retry_entry_i, view_load_retry_entry_depth;\n"
"static uint8_t picture_saved_window[50], picture_saved_cursor[2], picture_saved_pointer[2], picture_saved_end[2];\n"
"static uint16_t picture_var0_address;\n"
"static uint8_t picture_saved_var0;\n"
"static uint8_t picture_capture_bytes[51840];\n"
"static FILE *blit_failure_csv;\n"
"static uint8_t blit_saved_allocator[2160];\n"
"static uint8_t blit_saved_allocator_code[4];\n"
"static uint8_t blit_saved_after_set_loop_allocator[2160];\n"
"static uint8_t blit_saved_after_set_loop_code[4];\n"
"static uint8_t blit_fixture_allocator_before[2160];\n"
"#define BLIT_ALLOCATOR_TRACE_DEPTH 8\n"
"#define BLIT_ALLOCATOR_TRACE_PRINTS 24\n"
"#define BLIT_ALLOCATOR_TRACE_EVENTS 24\n"
"typedef struct { uint16_t entry_pc, return_pc, owner_tab; uint8_t return_sp, kind, bank, a, x, y, entry, view, loop, cel; uint32_t cycle; } BlitAllocatorTraceFrame;\n"
"typedef struct { uint32_t returned_address; uint16_t owner_tab, changed, allocated, freed, other, first_allocated, requested_blocks; uint8_t kind, entry, view, loop, cel, return_carry; } BlitAllocatorEvent;\n"
"static BlitAllocatorTraceFrame blit_allocator_trace_stack[BLIT_ALLOCATOR_TRACE_DEPTH];\n"
"static uint8_t blit_allocator_trace_maps[BLIT_ALLOCATOR_TRACE_DEPTH][2160];\n"
"static unsigned blit_allocator_trace_depth, blit_allocator_trace_calls, blit_allocator_trace_printed, blit_allocator_trace_overflow;\n"
"static BlitAllocatorEvent blit_allocator_events[BLIT_ALLOCATOR_TRACE_EVENTS];\n"
"static unsigned blit_allocator_event_count, blit_allocator_event_overflow;\n"
"static bool blit_failure_active, blit_set_loop_entered, blit_set_loop_returned, blit_second_allocator_entered;\n"
"static bool blit_set_loop_succeeded, blit_set_loop_bank_restored;\n"
"static bool blit_after_set_loop_state_saved, blit_retry_active;\n"
"static bool blit_bulk_helper_entered, blit_bulk_helper_returned, blit_c_allocator_returned;\n"
"static bool blit_partial_first_slot_seen, blit_partial_map_rolled_back, blit_partial_records_zero, blit_allocator_restored;\n"
"static bool blit_pending_parent_phase;\n"
"static bool blit_saved_view_flags, blit_loaded_resource;\n"
"static bool blit_saved_stale_counter;\n"
"static uint8_t blit_view_flags[2], blit_loaded_view_num;\n"
"static uint8_t blit_stale_counter;\n"
"static unsigned blit_load_view_entries, blit_set_view_entries, blit_draw_entries;\n"
"static int blit_failure_stage;\n"
"static uint16_t blit_return_pc, blit_set_loop_return_pc;\n"
"static uint16_t blit_retry_return_pc;\n"
"static uint16_t blit_bulk_return_pc, blit_c_allocator_return_pc;\n"
"static uint8_t blit_return_sp, blit_set_loop_return_sp, blit_entry_bank, blit_entry_rom, blit_entry_i, blit_set_loop_entry_bank;\n"
"static uint8_t blit_bulk_return_sp, blit_c_allocator_return_sp, blit_bulk_find_entries, blit_bulk_result, blit_c_allocator_result;\n"
"static uint32_t blit_partial_expected_address;\n"
"static uint8_t blit_entry_depth, blit_result, blit_agi_return_depth, blit_parent_return_depth, blit_nested_depth;\n"
"static bool blit_bank_preserved, blit_rom_preserved, blit_i_preserved;\n"
"static uint8_t blit_loop_addresses_unchanged, blit_loop_addresses_valid, blit_loop_addresses_reserved;\n"
"static uint8_t blit_backbuffers_unchanged, blit_backbuffers_valid, blit_backbuffers_reserved, blit_metadata_unchanged;\n"
"static uint8_t blit_retry_entered, blit_retry_result, blit_retry_set_loop_entries, blit_retry_loop_consistent;\n"
"static uint8_t blit_retry_backbuffers_valid, blit_retry_backbuffers_reserved, blit_retry_metadata_committed;\n"
"static uint8_t blit_retry_bank_preserved, blit_retry_rom_preserved, blit_retry_i_preserved, blit_retry_depth_restored;\n"
"static uint8_t blit_target_entry_num;\n"
"static unsigned blit_fixture_kind, blit_fixture_setup_entries, blit_fixture_find_entries, blit_fixture_delete_entries, blit_fixture_helper_entries, blit_fixture_release_entries, blit_fixture_owned_blocks, blit_fixture_released_blocks, blit_fixture_gap_blocks;\n"
"static bool blit_fixture_call_active, blit_fixture_cleanup_started, blit_fixture_cleanup_done, blit_failure_c_sp_preserved, blit_retry_c_sp_preserved;\n"
"static uint16_t blit_fixture_target, blit_fixture_return_pc, blit_fixture_return_sp, blit_fixture_c_sp, blit_entry_c_sp, blit_retry_entry_c_sp;\n"
"static uint32_t blit_fixture_gap_address;\n"
"static uint8_t blit_fixture_op, blit_fixture_saved_a, blit_fixture_saved_x, blit_fixture_saved_y, blit_fixture_saved_status, blit_fixture_saved_db, blit_fixture_saved_code_bank, blit_fixture_saved_e, blit_fixture_saved_ram_bank, blit_fixture_saved_rom_bank, blit_fixture_saved_depth;\n"
"static uint8_t blit_parent_viewtab_before[256], blit_parent_viewtab_snapshot;\n"
"static uint16_t blit_parent_viewtab_size;\n"
"static bool loader_init_active, loader_init_returned, loader_words_active, loader_words_returned, loader_open_active, loader_truncated; static unsigned loader_exit_seen; static uint16_t loader_exit_status; static uint8_t loader_exit_ram_bank,loader_exit_code_bank;\n"
"static unsigned loader_init_entries, loader_words_entries, loader_open_entries, loader_alloc_entries, loader_object_entries, loader_event_entries, loader_interpreter_entries, loader_irq_entries;\n"
"static uint16_t loader_init_return_pc, loader_words_return_pc, loader_open_return_pc;\n"
"static uint8_t loader_init_return_sp, loader_words_return_sp, loader_open_return_sp, loader_init_result, loader_words_result;\n"
"static bool loader_pass_active, loader_read_active, loader_seek_active;\n"
"static unsigned loader_pass_entries, loader_pass_returns, loader_read_calls, loader_read_bytes, loader_seek_calls;\n"
"static uint16_t loader_pass_return_pc, loader_read_return_pc, loader_seek_return_pc;\n"
"static uint8_t loader_pass_return_sp, loader_read_return_sp, loader_seek_return_sp, loader_pass_result, loader_read_result, loader_read_result_x, loader_seek_result, loader_seek_result_x;\n"
"static uint8_t loader_published_before[14], loader_allocator_before[232], loader_words_text_before[4500];\n"
"static unsigned loader_allocator_before_count;\n"
"enum { LOADER_SEARCH_COUNT = 9 };\n"
"static int16_t loader_search_results[LOADER_SEARCH_COUNT];\n"
"static unsigned loader_search_count;\n"
"#if LOADER_TEST_ENABLED\n"
"static unsigned loader_search_index;\n"
"static bool loader_search_active;\n"
"static uint16_t loader_search_return_pc, loader_search_return_sp, loader_search_return_c_sp;\n"
"static uint8_t loader_search_return_ram_bank, loader_search_return_rom_bank, loader_search_return_code_bank;\n"
"static uint8_t loader_search_return_status, loader_search_return_a, loader_search_return_x, loader_search_return_y, loader_search_return_e;\n"
"#endif\n"
"extern void shake_get_display(uint8_t *values);\n"
"extern void shake_get_i2c_state(uint8_t *values);\n"
"static uint16_t shake_call_return(uint8_t *return_sp);\n"
"static void blit_restore_viewtab_flags(void);\n"
"static void blit_snapshot_parent_viewtab(void);\n"
"static bool blit_parent_viewtab_unchanged(void);\n"
"static bool blit_original_reservations_preserved(const uint8_t *table);\n"
"static bool blit_successful_loop_reservations_preserved(const uint8_t *table);\n"
"static bool blit_parent_allocator_changes_valid(unsigned *final_new_blocks,unsigned *event_allocated_blocks,unsigned *find_events);\n"
"static unsigned blit_allocator_occupied_count(const uint8_t *table);\n"
"static bool blit_allocator_skip_code_valid(const uint8_t *before_code);\n"
"static void blit_report_parent_allocator_state(void);\n"
"static void blit_assert_partial_parent_return(void);\n"
"static void shake_push_test_jsr(uint16_t target);\n"
"static void text_case_start(unsigned index);\n"
"static void text_case_check_return(uint16_t pc);\n"
"static void text_case_track_source_helpers(uint16_t pc);\n"
"static void startup_text_check_frame_line(uint32_t frame, uint8_t row);\n"
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
"    foreground_entry_i=(regs.status&FLAG_INTERRUPT)!=0; foreground_entry_depth=shake_read(S__foregroundVeraDepth); foreground_irq_count=foreground_irq_max_cycles=0; foreground_max_irq_depth=0; foreground_active=true;\n"
"    if(kind==1 && PICTURE_TEST_ENABLED && (picture_test_stage==1 || picture_test_stage==3)) { picture_draw_active=true; picture_floodfill_count=picture_split_count=0; }\n"
"}\n"
"static void foreground_finish(void) {\n"
"    if(!FOREGROUND_TEST_ENABLED) return;\n"
"    uint32_t frames=shake_frames_seen-foreground_start_frames, serviced=foreground_vsync_counter()-foreground_start_counter;\n"
"    uint32_t missed=frames>serviced?frames-serviced:0; uint32_t cycles=clockticks6502-foreground_start_cycles;\n"
"    if(missed>0 && !(PICTURE_TEST_ENABLED && ((foreground_kind==1 && picture_irq_suppressed) || (foreground_kind==2 && picture_test_stage>=4)))) { fprintf(stderr,\"Production foreground caller %s lost %u VSYNC(s): cycles=%u asserted=%u serviced=%u\\n\",foreground_kind==1?\"b11DrawPic\":\"b9UpdateObjects\",missed,cycles,frames,serviced); exit(3); }\n"
"    uint8_t return_depth=shake_read(S__foregroundVeraDepth); if(return_depth!=foreground_entry_depth) { fprintf(stderr,\"Foreground VERA ownership did not return to its entry depth: entry=%u return=%u\\n\",foreground_entry_depth,return_depth); exit(3); }\n"
"    if(PICTURE_TEST_ENABLED && foreground_kind==2 && blit_pending_parent_phase && blit_failure_stage==2) blit_assert_partial_parent_return();\n"
"    if(PICTURE_TEST_ENABLED && foreground_kind==2 && blit_pending_parent_phase) { blit_parent_return_depth=shake_read(S__foregroundVeraDepth); if(blit_failure_stage<=2&&!blit_parent_viewtab_unchanged()) { fprintf(stderr,\"b9UpdateObjects changed caller ViewTable state after agiBlit allocation failure\\n\"); exit(3); } unsigned values[]={ (unsigned)blit_failure_stage,blit_set_loop_entered,blit_set_loop_returned,blit_set_loop_succeeded,blit_set_loop_bank_restored,blit_second_allocator_entered,blit_result,blit_entry_depth,blit_nested_depth,blit_agi_return_depth,blit_parent_return_depth,blit_bank_preserved,blit_rom_preserved,blit_i_preserved,blit_load_view_entries,blit_set_view_entries,blit_draw_entries,blit_loaded_resource,blit_loop_addresses_unchanged,blit_after_failure.loop_zero,blit_loop_addresses_valid,blit_loop_addresses_reserved,blit_backbuffers_unchanged,blit_after_failure.backbuffer_zero,blit_backbuffers_valid,blit_backbuffers_reserved,blit_metadata_unchanged,blit_retry_entered,blit_retry_result,blit_retry_set_loop_entries,blit_retry_loop_consistent,blit_retry_backbuffers_valid,blit_retry_backbuffers_reserved,blit_retry_metadata_committed,blit_retry_bank_preserved,blit_retry_rom_preserved,blit_retry_i_preserved,blit_retry_depth_restored }; for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++) fprintf(blit_failure_csv,\"%s%u\",i?\",\":\"\",values[i]); fputc('\\n',blit_failure_csv); fflush(blit_failure_csv); blit_pending_parent_phase=false; if(blit_failure_stage==1) blit_failure_stage=2; else if(blit_failure_stage==2) blit_failure_stage=3; else { blit_restore_viewtab_flags(); emu_write(5,0); raise(SIGINT); } }\n"
"    fprintf(foreground_csv,\"%d,%u,%u,%u,%u,%u,%u,%u\\n\",foreground_kind,cycles,frames,serviced,missed,foreground_irq_count,foreground_irq_max_cycles,foreground_entry_i);\n"
"    fflush(foreground_csv); foreground_active=false; foreground_kind=0;\n"
"    if(PICTURE_TEST_ENABLED && picture_draw_active) {\n"
"        const char *path=picture_draw_count==0?PICTURE_IRQ_VRAM:PICTURE_REFERENCE_VRAM;\n"
"        uint32_t hash=picture_capture(path);\n"
"        fprintf(picture_csv,\"%d,1,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\\n\",picture_draw_count+1,cycles,frames,serviced,missed,foreground_irq_count,foreground_irq_max_cycles,foreground_max_irq_depth,picture_floodfill_count,picture_split_count,foreground_entry_i,hash,picture_load_entries,picture_draw_entries);\n"
"        fflush(picture_csv); picture_draw_count++; picture_draw_active=false; picture_irq_suppressed=false;\n"
"    }\n"
"}\n"
"void shake_irq_accepted(void) {\n"
"    if(COMBINED_IRQ_TEST_ENABLED && combined_injected && !combined_complete && !combined_irq_active && !combined_irq_pending) { combined_accept_cycles=combined_row_start_cycles=combined_mask_start=clockticks6502; combined_mask_end=0; combined_i_contiguous=true; combined_i_pending_boundary_seen=false; combined_return_pc=regs.pc; combined_return_sp=(uint8_t)regs.sp; combined_irq_pending=true; }\n"
"    if(foreground_active && shake_read(S__foregroundVeraDepth)) {\n"
"        foreground_irq_active=true; foreground_irq_start_cycles=clockticks6502;\n"
"        foreground_irq_return_pc=regs.pc; foreground_irq_return_sp=(uint8_t)regs.sp;\n"
"        foreground_irq_a=regs.a; foreground_irq_x=regs.xl; foreground_irq_y=regs.yl; foreground_irq_status=regs.status&0xcf;\n"
"        foreground_irq_ram_bank=memory_get_ram_bank(); foreground_irq_rom_bank=memory_get_rom_bank();\n"
"        foreground_irq_depth=shake_read(S__foregroundVeraDepth);\n"
"        if(!foreground_irq_depth) { fprintf(stderr,\"Foreground IRQ ran without VERA ownership\\n\"); exit(3); }\n"
"        if(foreground_irq_depth>foreground_max_irq_depth) foreground_max_irq_depth=foreground_irq_depth;\n"
"        for(int i=0;i<4;i++) foreground_irq_vera[i]=foreground_vera_state(i);\n"
"        foreground_irq_fx[0]=video_get_dc_value(8); foreground_irq_fx[1]=video_get_dc_value(11); for(int i=0;i<4;i++) foreground_irq_fx[i+2]=video_get_dc_value(24+i); foreground_irq_fx_accum=video_get_fx_accum();\n"
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
"static void startup_text_check_frame(uint32_t frame, uint32_t cycle, uint16_t pc, uint8_t bank) {\n"
"    if (!startup_text_phase)\n"
"        return;\n"
"    startup_text_frame_samples++;\n"
"    if (startup_text_active) {\n"
"        unsigned lines = startup_text_phase < 13 ? startup_text_phase : 13;\n"
"        unsigned expected_row = 18u - startup_text_phase_call;\n"
"        unsigned expected_message = startup_text_phase - startup_text_phase_call;\n"
"        if (startup_text_phase > 23 || startup_text_phase_call >= lines ||\n"
"            !expected_message || expected_message > 23 ||\n"
"            startup_text_row != expected_row || startup_text_column != 12 ||\n"
"            !startup_text_message || startup_text_message > 23 ||\n"
"            startup_text_message != expected_message) {\n"
"            fprintf(stderr, \"KQ1 startup frame phase/call state mismatch: phase=%u completed_calls=%u row/column/message=%u/%u/%u expected=%u/12/%u frame=%u PC=%04x RAMbank=%u\\n\", startup_text_phase, startup_text_phase_call, startup_text_row, startup_text_column, startup_text_message, expected_row, expected_message, frame, pc, bank);\n"
"            exit(3);\n"
"        }\n"
"    }\n"
"    for (unsigned row = 6; row <= 18; row++)\n"
"        startup_text_check_frame_line(frame, (uint8_t)row);\n"
"    unsigned visible = 0;\n"
"    for (unsigned row = 6; row <= 18; row++)\n"
"        for (unsigned column = 12; column <= 29; column++) {\n"
"            uint32_t address = 0xda00u + (row + 3u) * 128u + column * 2u;\n"
"            uint8_t tile = video_space_read(address);\n"
"            if (tile == 0x80)\n"
"                continue;\n"
"            visible++;\n"
"            uint8_t attribute = video_space_read(address + 1u);\n"
"            unsigned palette = attribute >> 4;\n"
"            uint32_t palette_address = 0x1fa00u + palette * 32u;\n"
"            uint16_t foreground =\n"
"                (uint16_t)(video_space_read(palette_address + 2u) |\n"
"                           ((uint16_t)video_space_read(palette_address + 3u) << 8));\n"
"            uint16_t background =\n"
"                (uint16_t)(video_space_read(palette_address + 4u) |\n"
"                           ((uint16_t)video_space_read(palette_address + 5u) << 8));\n"
"            if (foreground != 0x0fffu || background != 0) {\n"
"                fprintf(\n"
"                    stderr,\n"
"                    \"KQ1 startup frame RGB regression: vsync_frame=%u cycle=%u phase=%u returns=%u row=%u column=%u tile=%02x attribute=%02x palette=%u foregroundRGB=%04x backgroundRGB=%04x expectedForegroundRGB=0fff expectedBackgroundRGB=0000 PC=%04x RAMbank=%u\\n\",\n"
"                    frame, cycle, startup_text_phase, startup_text_returns, row, column, tile,\n"
"                    attribute, palette, foreground, background, pc, bank);\n"
"                exit(3);\n"
"            }\n"
"        }\n"
"    if (visible)\n"
"        startup_text_visible_frame_samples++;\n"
"}\n"
"void shake_vsync_asserted(void) {\n"
"    shake_vsync = clockticks6502;\n"
"    shake_frames_seen++;\n"
"    if (STARTUP_TEXT_TEST_ENABLED) {\n"
"        uint32_t address = 0xda00u + 21u * 128u + 12u * 2u;\n"
"        unsigned column = 12;\n"
"        if (!startup_text_negative_control_injected && startup_text_returns &&\n"
"            (!startup_text_active || startup_text_row != 18)) {\n"
"            while (column < 28) {\n"
"                uint8_t tile = video_space_read(address);\n"
"                if (tile != ' ' && tile != 0x80)\n"
"                    break;\n"
"                column++;\n"
"                address += 2;\n"
"            }\n"
"            if (column < 28) {\n"
"                uint8_t original = video_space_read(address);\n"
"                uint8_t corrupted = original ^ 1u;\n"
"                startup_text_negative_control_row = 18;\n"
"                startup_text_negative_control_column = (uint8_t)column;\n"
"                startup_text_negative_control_original = original;\n"
"                startup_text_negative_control_injected = true;\n"
"                startup_text_negative_control_active = true;\n"
"                video_space_write(address, corrupted);\n"
"                fprintf(stderr, \"KQ1 startup glyph negative control injected: vsync_frame=%u row=18 column=%u original=%02x corrupted=%02x\\n\", shake_frames_seen, column, original, corrupted);\n"
"                startup_text_check_frame(shake_frames_seen, clockticks6502, regs.pc, memory_get_ram_bank());\n"
"                if (!startup_text_negative_control_rejected) {\n"
"                    fprintf(stderr, \"KQ1 startup glyph negative control was not rejected at its target cell\\n\");\n"
"                    exit(3);\n"
"                }\n"
"                video_space_write(address, original);\n"
"                startup_text_negative_control_active = false;\n"
"                startup_text_check_frame_line(shake_frames_seen, startup_text_negative_control_row);\n"
"                fprintf(stderr, \"KQ1 startup glyph negative control restored: vsync_frame=%u row=%u column=%u glyph=%02x\\n\", shake_frames_seen, startup_text_negative_control_row, startup_text_negative_control_column, original);\n"
"            } else {\n"
"                startup_text_check_frame(shake_frames_seen, clockticks6502, regs.pc, memory_get_ram_bank());\n"
"            }\n"
"        } else {\n"
"            startup_text_check_frame(shake_frames_seen, clockticks6502, regs.pc, memory_get_ram_bank());\n"
"        }\n"
"    }\n"
"}\n"
"void shake_video_frame_written(uint32_t emulator_frame, uint32_t cycle, uint16_t pc, uint8_t bank) {\n"
"    if (!shake_video_trace) {\n"
"        shake_video_trace = fopen(SHAKE_VIDEO_CSV, \"w\");\n"
"        if (!shake_video_trace) {\n"
"            perror(\"video frame trace\");\n"
"            exit(2);\n"
"        }\n"
"        fprintf(\n"
"            shake_video_trace,\n"
"            \"gif_frame,emulator_frame,cycle,asserted_frames,command_index,shake_count,shake_phases,shake_frames,hstart,hstop,vstart,vstop,border,pc,bank,b9_entries,foreground_active,foreground_kind\\n\");\n"
"    }\n"
"    uint8_t display[5];\n"
"    shake_get_display(display);\n"
"    fprintf(shake_video_trace, \"%u,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d\\n\",\n"
"            shake_video_frame_index++, emulator_frame, cycle, shake_frames_seen,\n"
"            shake_command_index, shake_read(S_shakeCount), shake_read(S_shakePhases),\n"
"            shake_read(S_shakeFrames), display[0], display[1], display[2], display[3], display[4],\n"
"            (unsigned)pc, bank, shake_real_b9_entries, foreground_active, foreground_kind);\n"
"    fflush(shake_video_trace);\n"
"}\n"
"void shake_foreground_vram_write(uint32_t address) { if(foreground_irq_active&&(address<0x1f9c0||address>=0x1f9d0)) { fprintf(stderr,\"KERNAL IRQ wrote VERA memory at $%05x during renderer ownership\\n\",address); exit(3); } }\n"
"void shake_foreground_vera_register_write(uint8_t reg,uint8_t dcsel,uint8_t value,uint8_t oldvalue,uint16_t pc,uint8_t codebank,uint8_t rambank) { (void)dcsel; (void)oldvalue; (void)pc; (void)codebank; (void)rambank; if(foreground_irq_active&&reg==5&&value!=0&&value!=1&&value!=4&&value!=foreground_irq_vera[0]) { fprintf(stderr,\"Foreground sound selected an unexpected VERA register bank\\n\"); exit(3); } }\n"
"void shake_window_write(void) {\n"
"    if(shake_irq_active) {\n"
"        shake_write_pending=true; shake_window_writes++; shake_total_writes++;\n"
"    }\n"
"}\n"
"void shake_psg_write(uint8_t reg,uint8_t value) { if(shake_irq_active) { shake_psg_writes++; if((reg&3)==2 && (value&63) && (value&192)) shake_audible_writes++; } if(combined_irq_active && combined_sound_active) { combined_psg_writes++; if((reg&3)==2 && (value&63) && (value&192)) combined_audible_psg_writes++; } }\n"
"void shake_sprite_write(uint32_t address,uint8_t value) { if(shake_irq_active) shake_sprite_writes++; if(combined_irq_active && combined_sprite_active) combined_sprite_vram_writes++; if(shake_sprite_batch_handler_active&&address>=0x1fc00&&address<0x20000) { unsigned offset=address-0x1fc00; shake_sprite_attr_actual[offset]=value; shake_sprite_attr_written[offset]=1; } }\n"
"void shake_combined_vram_write(uint32_t address, uint8_t value) {\n"
"    if (text_case_vram_trace)\n"
"        text_case_vram_write(address, value);\n"
"    if (combined_guard_trace_active && address == (uint32_t)combined_text_destination + 2 * 513) {\n"
"        combined_guard_writes++;\n"
"        combined_guard_last_pc = regs.pc;\n"
"        combined_guard_last_bank = memory_get_ram_bank();\n"
"        combined_guard_last_value = value;\n"
"        if (!combined_guard_seed_active && combined_guard_seeded)\n"
"            combined_guard_post_seed_writes++;\n"
"        if (!combined_guard_seed_active && combined_menu_active && value == 0x80) {\n"
"            combined_guard_menu_clear_seen = true;\n"
"            combined_guard_menu_clear_writes++;\n"
"        }\n"
"        fprintf(\n"
"            stderr,\n"
"            \"Combined text guard write: value=%02x PC=%04x bank=%02x cycle=%u menu=%u text=%u fixture_seed=%u post_seed=%u\\n\",\n"
"            value, regs.pc, memory_get_ram_bank(), clockticks6502, combined_menu_active,\n"
"            combined_text_active, combined_guard_seed_active, combined_guard_post_seed_writes);\n"
"    }\n"
"    if (!combined_irq_active)\n"
"        return;\n"
"    if (combined_menu_active)\n"
"        combined_menu_vram_writes++;\n"
"    if (!combined_text_active)\n"
"        return;\n"
"    uint32_t guard = (uint32_t)combined_text_destination + 2 * 513;\n"
"    if (combined_text_attribute_pending && address != combined_text_pending_attribute_address) {\n"
"        fprintf(\n"
"            stderr,\n"
"            \"Combined text glyph was not immediately followed by its attribute: glyph=%05x expected-attribute=%05x next-write=%05x value=%02x cycle=%u\\n\",\n"
"            combined_text_last_write_address, combined_text_pending_attribute_address, address,\n"
"            value, clockticks6502);\n"
"        exit(3);\n"
"    }\n"
"    if (address < combined_text_destination || address > guard)\n"
"        return;\n"
"    uint8_t actual = video_space_read(address);\n"
"    if (address == guard) {\n"
"        fprintf(stderr,\n"
"                \"Combined text crossed its guarded VERA cell: address=%05x value=%02x cycle=%u\\n\",\n"
"                address, value, clockticks6502);\n"
"        exit(3);\n"
"    }\n"
"    if ((address - combined_text_destination) & 1u) {\n"
"        uint8_t palette = (uint8_t)(shake_banked_read(S__b3PaletteNumber, 3) << 4);\n"
"        if (!combined_text_attribute_pending ||\n"
"            address != combined_text_pending_attribute_address ||\n"
"            value != combined_text_pending_attribute_value || value != palette || actual != value) {\n"
"            fprintf(\n"
"                stderr,\n"
"                \"Combined text attribute was not paired with its glyph/palette: address=%05x expected-address=%05x value=%02x expected=%02x palette=%02x stored=%02x attributes=%u cycle=%u\\n\",\n"
"                address, combined_text_pending_attribute_address, value,\n"
"                combined_text_pending_attribute_value, palette, actual,\n"
"                combined_text_attribute_writes, clockticks6502);\n"
"            exit(3);\n"
"        }\n"
"        combined_text_attribute_pending = false;\n"
"        combined_text_attribute_writes++;\n"
"        combined_text_last_attribute_write_address = (uint16_t)address;\n"
"        combined_text_last_attribute_write_cycle = clockticks6502;\n"
"        return;\n"
"    }\n"
"    uint32_t expected = (uint32_t)combined_text_destination + 2 * combined_text_total_writes;\n"
"    if (combined_text_attribute_pending || address != expected || value != 'K' || actual != value) {\n"
"        fprintf(\n"
"            stderr,\n"
"            \"Combined text store was not the expected committed VERA character: address=%05x expected=%05x value=%02x actual=%02x write=%u pending-attribute=%u cycle=%u\\n\",\n"
"            address, expected, value, actual, combined_text_total_writes,\n"
"            combined_text_attribute_pending, clockticks6502);\n"
"        exit(3);\n"
"    }\n"
"    combined_text_vram_writes++;\n"
"    combined_text_total_writes++;\n"
"    combined_text_last_write_address = (uint16_t)address;\n"
"    combined_text_last_write_value = value;\n"
"    combined_text_last_write_cycle = clockticks6502;\n"
"    combined_text_pending_attribute_address = (uint16_t)(address + 1u);\n"
"    combined_text_pending_attribute_value =\n"
"        (uint8_t)(shake_banked_read(S__b3PaletteNumber, 3) << 4);\n"
"    combined_text_attribute_pending = true;\n"
"}\n"
"void shake_text_source_read(uint16_t address,uint8_t bank,uint16_t instruction,uint8_t codebank) { if(!text_case_active||!text_case_source_trace||!text_source_depth||(address==instruction&&bank==codebank)) return; uint32_t first=text_case_source,last=first+text_case_source_bytes+text_case_source_guard; if((uint32_t)address<first||(uint32_t)address>=last) return; if(memory_get_ram_bank()!=text_case_source_bank) return; if(address>=0xc000) text_case_c000_reads++; if((uint32_t)address<first+text_case_source_bytes) text_case_source_reads++; else text_case_source_oob_reads++; }\n"
"static void text_case_vram_write(uint32_t address,uint8_t value) { uint32_t base=0xdb80u; unsigned trace_rows=text_case_index==TEXT_CASE_FULL_CLEAR?29:(text_case_index==TEXT_CASE_OVERLAP?4:2); if(address<base||address>=base+trace_rows*128u||((address-base)&1u)) return; text_case_char_writes++; text_case_last_write_address=(uint16_t)address; text_case_last_write_value=value; text_case_last_write_cycle=clockticks6502; if(video_space_read(address)!=value) { fprintf(stderr,\"Text case VERA write was not stored: address=%05x value=%02x stored=%02x cycle=%u\\n\",address,value,video_space_read(address),clockticks6502); exit(3); } }\n"
"static void shake_banked_ram(uint16_t address,uint8_t bank,uint8_t value) {\n"
"    uint8_t old=memory_get_ram_bank(); memory_set_ram_bank(bank); shake_ram(address,value); memory_set_ram_bank(old);\n"
"}\n"
"static uint8_t shake_banked_read(uint16_t address,uint8_t bank) { uint8_t old=memory_get_ram_bank(); memory_set_ram_bank(bank); uint8_t value=shake_read(address); memory_set_ram_bank(old); return value; }\n"
"static uint16_t shake_pointer_value(uint16_t address) { return (uint16_t)(shake_banked_read(address,0)|((uint16_t)shake_banked_read(address+1,0)<<8)); }\n"
"static uint16_t shake_var_address(uint8_t index) { return (uint16_t)(shake_pointer_value(S__var)+index); }\n"
"static uint16_t shake_flag_address(uint8_t index) { return (uint16_t)(shake_pointer_value(S__flag)+index); }\n"
"static uint16_t combined_read16(uint16_t address,uint8_t bank) { return (uint16_t)(shake_banked_read(address,bank)|((uint16_t)shake_banked_read(address+1,bank)<<8)); }\n"
"static void combined_check_multiply_results(void) { static const uint8_t values[]={2,0,1,14,15,16,17,18,255}; for(unsigned row=0;row<sizeof(values);row++) for(unsigned column=0;column<sizeof(values);column++) { uint8_t left=values[row],right=values[column]; uint16_t expected=(uint16_t)((uint16_t)left*(uint16_t)right); uint16_t actual=combined_read16(S__testMultiplyBoundsResults+2*(row*sizeof(values)+column),0); if(actual!=expected) { fprintf(stderr,\"MULT_8x8_16 regression: A=%u Y=%u got=%u expected=%u\\n\",left,right,actual,expected); exit(3); } } fprintf(stderr,\"MULT_8x8_16 boundary grid passed: 81 products checked with 16-bit oracle\\n\"); }\n"
"static const char *const startup_text_messages[23]={\"Designed and    \",\"    Written by: \",\"Roberta Williams\",\"                \",\"Orig. Version by\",\" Charles Tingley\",\"  Ken MacNeill  \",\"   Chris Iden   \",\"                \",\"Graphics by:    \",\" Doug MacNeill  \",\"  Greg Rowland  \",\"                \",\"New Version by: \",\" Jeff Stephenson\",\"  Sol Ackerman  \",\"   Chris Iden   \",\"                \",\"Thanks to:      \",\" Linda Ackerman  \",\"  Mark Crowe     \",\"   Robert Heitman\",\"    Scott Murphy \"};\n"
"static void startup_text_init_published_state(void) {\n"
"    if (startup_text_published_initialized)\n"
"        return;\n"
"    for (unsigned row = 0; row < 13; row++) {\n"
"        startup_text_published_messages[row] = 0;\n"
"        startup_text_published_widths[row] = 0;\n"
"        for (unsigned i = 0; i < 18; i++)\n"
"            startup_text_published_glyphs[row][i] = 0x80;\n"
"    }\n"
"    startup_text_published_initialized = true;\n"
"}\n"
"static void startup_text_publish_line(uint8_t row, uint8_t message) {\n"
"    unsigned slot = row - 6;\n"
"    const char *line = startup_text_messages[message - 1];\n"
"    unsigned width = (unsigned)strlen(line);\n"
"    for (unsigned i = 0; i < width; i++)\n"
"        startup_text_published_glyphs[slot][i] = (uint8_t)line[i];\n"
"    if (width > startup_text_published_widths[slot])\n"
"        startup_text_published_widths[slot] = (uint8_t)width;\n"
"    startup_text_published_messages[slot] = message;\n"
"}\n"
"static void startup_text_check_line(uint8_t logical_row, uint8_t column, uint8_t message) {\n"
"    if (!logical_row || !message || message > 23) {\n"
"        fprintf(stderr, \"Invalid startup text expectation row=%u column=%u message=%u\\n\",\n"
"                logical_row, column, message);\n"
"        exit(3);\n"
"    }\n"
"    uint32_t address = 0xda00u + (uint32_t)(logical_row + 3u) * 128u + (uint32_t)column * 2u;\n"
"    const char *expected = startup_text_messages[message - 1];\n"
"    for (unsigned i = 0; i < strlen(expected); i++) {\n"
"        uint8_t actual = video_space_read(address + i * 2u), want = (uint8_t)expected[i];\n"
"        if (actual != want) {\n"
"            fprintf(\n"
"                stderr,\n"
"                \"KQ1 startup credit mismatch: phase=%u call=%u row=%u column=%u message=%u cell=%u VERA=%05x got=%02x expected=%02x\\n\",\n"
"                startup_text_phase, startup_text_phase_call, logical_row, column, message, i,\n"
"                address + i * 2u, actual, want);\n"
"            exit(3);\n"
"        }\n"
"    }\n"
"}\n"
"static void startup_text_check_blank_line(uint8_t logical_row) {\n"
"    for (unsigned column = 12; column <= 29; column++) {\n"
"        uint32_t address = 0xda00u + (uint32_t)(logical_row + 3u) * 128u + column * 2u;\n"
"        uint8_t actual = video_space_read(address);\n"
"        if (actual != 0x80) {\n"
"            fprintf(\n"
"                stderr,\n"
"                \"KQ1 startup blank credit row mismatch: phase=%u row=%u column=%u VERA=%05x got=%02x expected=80\\n\",\n"
"                startup_text_phase, logical_row, column, address, actual);\n"
"            exit(3);\n"
"        }\n"
"    }\n"
"}\n"
"static void startup_text_check_fixed(uint8_t logical_row, uint8_t column, const char *expected) {\n"
"    uint32_t address = 0xda00u + (uint32_t)(logical_row + 3u) * 128u + (uint32_t)column * 2u;\n"
"    for (unsigned i = 0; i < strlen(expected); i++) {\n"
"        uint8_t actual = video_space_read(address + i * 2u), want = (uint8_t)expected[i];\n"
"        if (actual != want) {\n"
"            fprintf(\n"
"                stderr,\n"
"                \"KQ1 startup fixed text mismatch: row=%u column=%u cell=%u VERA=%05x got=%02x expected=%02x\\n\",\n"
"                logical_row, column, i, address + i * 2u, actual, want);\n"
"            exit(3);\n"
"        }\n"
"    }\n"
"}\n"
"static void startup_text_check_phase(void) {\n"
"    unsigned lines = startup_text_phase < 13 ? startup_text_phase : 13;\n"
"    for (unsigned i = 0; i < lines; i++)\n"
"        startup_text_check_line((uint8_t)(18u - i), 12, (uint8_t)(startup_text_phase - i));\n"
"    for (unsigned row = 6; row <= 18u - lines; row++)\n"
"        startup_text_check_blank_line((uint8_t)row);\n"
"    uint32_t copyright = 0xda00u + 25u * 128u + 2u;\n"
"    uint32_t prompt = 0xda00u + 27u * 128u + 10u;\n"
"    if (!startup_text_fixed_rows_seen && video_space_read(copyright) == '(' &&\n"
"        video_space_read(prompt) == 'P') {\n"
"        startup_text_fixed_rows_seen = true;\n"
"        fprintf(stderr, \"KQ1 startup fixed title text became visible by phase %u\\n\",\n"
"                startup_text_phase);\n"
"    }\n"
"    if (startup_text_fixed_rows_seen) {\n"
"        startup_text_check_fixed(22, 1, \"(c) copyright SIERRA On-Line Inc.  1987\");\n"
"        startup_text_check_fixed(24, 5, \"Press any key to continue.\");\n"
"    } else if (startup_text_phase == 23) {\n"
"        fprintf(stderr, \"KQ1 startup fixed title text never appeared: copyright=%02x prompt=%02x\\n\",\n"
"                video_space_read(copyright), video_space_read(prompt));\n"
"        exit(3);\n"
"    }\n"
"}\n"
"static uint32_t startup_text_cell_address(uint8_t row, unsigned index) {\n"
"    return 0xda00u + (uint32_t)(row + 3u) * 128u + (12u + index) * 2u;\n"
"}\n"
"static bool startup_text_line_progress_matches(uint8_t row, uint8_t pending_message) {\n"
"    if (!pending_message || pending_message > 23)\n"
"        return false;\n"
"    unsigned slot = row - 6;\n"
"    const char *next = startup_text_messages[pending_message - 1];\n"
"    unsigned next_width = (unsigned)strlen(next);\n"
"    unsigned old_width = startup_text_published_widths[slot];\n"
"    unsigned width = next_width > old_width ? next_width : old_width;\n"
"    if (video_space_read(startup_text_cell_address(row, width)) != 0x80)\n"
"        return false;\n"
"    for (unsigned prefix = 0; prefix <= next_width; prefix++) {\n"
"        bool write_old = true;\n"
"        bool write_blank = true;\n"
"        bool clear_old = true;\n"
"        bool clear_blank = true;\n"
"        for (unsigned i = 0; i < width; i++) {\n"
"            uint8_t actual = video_space_read(startup_text_cell_address(row, i));\n"
"            uint8_t old = startup_text_published_glyphs[slot][i];\n"
"            uint8_t new_glyph = i < next_width ? (uint8_t)next[i] : 0x80;\n"
"            if (i < prefix) {\n"
"                if (actual != new_glyph) {\n"
"                    write_old = false;\n"
"                    write_blank = false;\n"
"                }\n"
"                if (actual != 0x80) {\n"
"                    clear_old = false;\n"
"                    clear_blank = false;\n"
"                }\n"
"            } else {\n"
"                if (actual != old) {\n"
"                    write_old = false;\n"
"                    clear_old = false;\n"
"                }\n"
"                if (actual != 0x80) {\n"
"                    write_blank = false;\n"
"                    clear_blank = false;\n"
"                }\n"
"            }\n"
"        }\n"
"        if (write_old || write_blank || clear_old || clear_blank)\n"
"            return true;\n"
"    }\n"
"    return false;\n"
"}\n"
"static void startup_text_frame_glyph_error(\n"
"    uint32_t frame, uint8_t row, unsigned index, uint8_t actual, uint8_t expected,\n"
"    uint8_t published_message, uint8_t pending_message, bool active) {\n"
"    if (startup_text_negative_control_active &&\n"
"        !startup_text_negative_control_rejected &&\n"
"        row == startup_text_negative_control_row &&\n"
"        12u + index == startup_text_negative_control_column &&\n"
"        expected == startup_text_negative_control_original &&\n"
"        actual == (uint8_t)(startup_text_negative_control_original ^ 1u) &&\n"
"        published_message && !pending_message && !active) {\n"
"        fprintf(\n"
"            stderr,\n"
"            \"KQ1 startup glyph negative control rejected: vsync_frame=%u phase=%u completed_calls=%u row=%u column=%u published_message=%u got=%02x expected=%02x\\n\",\n"
"            frame, startup_text_phase, startup_text_phase_call, row, 12u + index,\n"
"            published_message, actual, expected);\n"
"        startup_text_negative_control_rejected = true;\n"
"        return;\n"
"    }\n"
"    if (active) {\n"
"        fprintf(\n"
"            stderr,\n"
"            \"KQ1 startup frame glyph regression: vsync_frame=%u phase=%u completed_calls=%u row=%u column=%u got=%02x expected=valid-slice published_message=%u pending_message=%u\\n\",\n"
"            frame, startup_text_phase, startup_text_phase_call, row, 12u + index,\n"
"            actual, published_message, pending_message);\n"
"    } else {\n"
"        fprintf(\n"
"            stderr,\n"
"            \"KQ1 startup frame glyph regression: vsync_frame=%u phase=%u completed_calls=%u row=%u column=%u got=%02x expected=%02x published_message=%u\\n\",\n"
"            frame, startup_text_phase, startup_text_phase_call, row, 12u + index,\n"
"            actual, expected, published_message);\n"
"    }\n"
"    exit(3);\n"
"}\n"
"static void startup_text_check_frame_line(uint32_t frame, uint8_t row) {\n"
"    unsigned slot = row - 6;\n"
"    uint8_t published = startup_text_published_messages[slot];\n"
"    bool active = startup_text_active && row == startup_text_row;\n"
"    if (active) {\n"
"        if (startup_text_line_progress_matches(row, startup_text_message))\n"
"            return;\n"
"        unsigned next_width =\n"
"            (unsigned)strlen(startup_text_messages[startup_text_message - 1]);\n"
"        unsigned width =\n"
"            next_width > startup_text_published_widths[slot]\n"
"                ? next_width\n"
"                : startup_text_published_widths[slot];\n"
"        unsigned index = 0;\n"
"        const char *next = startup_text_messages[startup_text_message - 1];\n"
"        for (unsigned i = 0; i < width; i++) {\n"
"            uint8_t actual = video_space_read(startup_text_cell_address(row, i));\n"
"            uint8_t new_glyph = i < next_width ? (uint8_t)next[i] : 0x80;\n"
"            if (actual != new_glyph &&\n"
"                actual != startup_text_published_glyphs[slot][i] &&\n"
"                actual != 0x80) {\n"
"                index = i;\n"
"                break;\n"
"            }\n"
"        }\n"
"        uint8_t expected =\n"
"            index < next_width\n"
"                ? (uint8_t)next[index]\n"
"                : startup_text_published_glyphs[slot][index];\n"
"        uint8_t actual =\n"
"            video_space_read(startup_text_cell_address(row, index));\n"
"        startup_text_frame_glyph_error(\n"
"            frame, row, index, actual, expected, published,\n"
"            startup_text_message, true);\n"
"        return;\n"
"    }\n"
"    unsigned width = startup_text_published_widths[slot];\n"
"    if (published) {\n"
"        for (unsigned i = 0; i < width; i++) {\n"
"            uint8_t actual = video_space_read(startup_text_cell_address(row, i));\n"
"            uint8_t expected = startup_text_published_glyphs[slot][i];\n"
"            if (actual != expected) {\n"
"                startup_text_frame_glyph_error(\n"
"                    frame, row, i, actual, expected, published, 0, false);\n"
"                return;\n"
"            }\n"
"        }\n"
"        uint8_t tail = video_space_read(startup_text_cell_address(row, width));\n"
"        if (tail != 0x80)\n"
"            startup_text_frame_glyph_error(\n"
"                frame, row, width, tail, 0x80, published, 0, false);\n"
"        return;\n"
"    }\n"
"    for (unsigned i = 0; i < 18; i++) {\n"
"        uint8_t actual = video_space_read(startup_text_cell_address(row, i));\n"
"        if (actual != 0x80) {\n"
"            startup_text_frame_glyph_error(\n"
"                frame, row, i, actual, 0x80, 0, 0, false);\n"
"            return;\n"
"        }\n"
"    }\n"
"}\n"
"static bool startup_graphics_active; static unsigned startup_graphics_checks; static uint16_t startup_graphics_return_pc, startup_graphics_entry_c_sp; static uint8_t startup_graphics_return_sp; static uint32_t startup_graphics_entry_cycle;\n"
"static void startup_graphics_abi_check(uint16_t pc, uint8_t bank) {\n"
"    if (startup_graphics_active && pc == startup_graphics_return_pc &&\n"
"        (uint8_t)regs.sp == startup_graphics_return_sp) {\n"
"        uint16_t return_c_sp = combined_read16(S_c_sp, 0);\n"
"        if (return_c_sp != startup_graphics_entry_c_sp) {\n"
"            fprintf(\n"
"                stderr,\n"
"                \"KQ1 startup ABI regression: real b6InitGraphics return changed C stack pointer entry=%04x return=%04x caller=%04x entry_cycle=%u return_cycle=%u RAMbank=%u PBR=%u\\n\",\n"
"                startup_graphics_entry_c_sp, return_c_sp,\n"
"                (uint16_t)(startup_graphics_return_pc - 1u), startup_graphics_entry_cycle,\n"
"                clockticks6502, bank, regs.k);\n"
"            exit(3);\n"
"        }\n"
"        startup_graphics_active = false;\n"
"        startup_graphics_checks++;\n"
"        fprintf(stderr, \"KQ1 startup ABI check passed: b6InitGraphics preserved c_sp=%04x\\n\",\n"
"                return_c_sp);\n"
"    }\n"
"    if (!startup_graphics_active && bank == 6 && pc == S__b6InitGraphics) {\n"
"        startup_graphics_return_pc = shake_call_return(&startup_graphics_return_sp);\n"
"        startup_graphics_entry_c_sp = combined_read16(S_c_sp, 0);\n"
"        startup_graphics_entry_cycle = clockticks6502;\n"
"        startup_graphics_active = true;\n"
"    }\n"
"}\n"
"static void startup_text_before_step(uint16_t pc, uint8_t bank) {\n"
"    if (startup_text_active && pc == startup_text_return_pc &&\n"
"        (uint8_t)regs.sp == startup_text_return_sp) {\n"
"        uint16_t generation = combined_read16(S_irqCompletionGeneration, 6);\n"
"        uint8_t command = shake_banked_read(S_sendIrqCommand, 6),\n"
"                work = shake_banked_read(S_irqWorkKind, 6);\n"
"        if (memory_get_ram_bank() != startup_text_entry_ram_bank ||\n"
"            generation != (uint16_t)(startup_text_generation + 1) || command || work) {\n"
"            fprintf(\n"
"                stderr,\n"
"                \"KQ1 startup display.v did not return after exactly one command completion: call=%u bank=%u/%u generation=%04x expected=%04x command/work=%u/%u\\n\",\n"
"                startup_text_returns + 1, memory_get_ram_bank(), startup_text_entry_ram_bank,\n"
"                generation, (uint16_t)(startup_text_generation + 1), command, work);\n"
"            exit(3);\n"
"        }\n"
"        startup_text_active = false;\n"
"        startup_text_returns++;\n"
"        startup_text_phase_call++;\n"
"        uint8_t expected_message = (uint8_t)(startup_text_phase - startup_text_phase_call + 1),\n"
"                expected_row = (uint8_t)(18u - startup_text_phase_call + 1);\n"
"        if (startup_text_message != expected_message || startup_text_row != expected_row ||\n"
"            startup_text_column != 12) {\n"
"            fprintf(\n"
"                stderr,\n"
"                \"KQ1 startup Logic83 argument sequence mismatch: phase=%u call=%u row/column/message=%u/%u/%u expected=%u/12/%u\\n\",\n"
"                startup_text_phase, startup_text_phase_call, startup_text_row, startup_text_column,\n"
"                startup_text_message, expected_row, expected_message);\n"
"            exit(3);\n"
"        }\n"
"        uint32_t call_cycles = clockticks6502 - startup_text_call_cycles,\n"
"                 call_frames = shake_frames_seen - startup_text_call_frames;\n"
"        fprintf(\n"
"            stderr,\n"
"            \"KQ1 Logic83 display.v phase=%u call=%u row=%u message=%u cycle_span=%u frame_span=%u generation=%04x->%04x\\n\",\n"
"            startup_text_phase, startup_text_phase_call, startup_text_row, startup_text_message,\n"
"            call_cycles, call_frames, startup_text_generation, generation);\n"
"        startup_text_check_line(startup_text_row, startup_text_column, startup_text_message);\n"
"        startup_text_publish_line(startup_text_row, startup_text_message);\n"
"        if (startup_text_phase_call == (startup_text_phase < 13 ? startup_text_phase : 13)) {\n"
"            startup_text_check_phase();\n"
"            startup_text_phase_cycles = clockticks6502 - startup_text_phase_start_cycles;\n"
"            startup_text_phase_frames = shake_frames_seen - startup_text_phase_start_frames;\n"
"            fprintf(\n"
"                stderr,\n"
"                \"KQ1 Logic83 startup credit phase %u passed (lines=%u, real display.v calls=%u, cycle_span=%u, frame_span=%u)\\n\",\n"
"                startup_text_phase, startup_text_phase_call, startup_text_returns,\n"
"                startup_text_phase_cycles, startup_text_phase_frames);\n"
"            startup_text_phase_call = 0;\n"
"            if (startup_text_phase == 23) {\n"
"                if (startup_text_returns != 221) {\n"
"                    fprintf(\n"
"                        stderr,\n"
"                        \"KQ1 startup credit sequence ended after %u display.v calls, expected 221\\n\",\n"
"                        startup_text_returns);\n"
"                    exit(3);\n"
"                }\n"
"                if (startup_graphics_checks != 1) {\n"
"                    fprintf(stderr, \"KQ1 startup ABI check ran %u times, expected exactly once\\n\",\n"
"                            startup_graphics_checks);\n"
"                    exit(3);\n"
"                }\n"
"                uint32_t startup_text_vsync_frames = shake_frames_seen - startup_text_total_frames;\n"
"                if (startup_text_frame_samples != startup_text_vsync_frames) {\n"
"                    fprintf(\n"
"                        stderr,\n"
"                        \"KQ1 startup RGB sample coverage mismatch: VSYNC frames=%u RGB samples=%u visible=%u\\n\",\n"
"                        startup_text_vsync_frames, startup_text_frame_samples,\n"
"                        startup_text_visible_frame_samples);\n"
"                    exit(3);\n"
"                }\n"
"                if (!startup_text_frame_samples || !startup_text_visible_frame_samples) {\n"
"                    fprintf(\n"
"                        stderr,\n"
"                        \"KQ1 startup RGB VSYNC observer saw no visible credits: VSYNC samples=%u with-visible-credits=%u\\n\",\n"
"                        startup_text_frame_samples, startup_text_visible_frame_samples);\n"
"                    exit(3);\n"
"                }\n"
"                fprintf(\n"
"                    stderr,\n"
"                    \"Production KQ1 startup text passed: 23 Logic83 phases, 221 display.v returns, 13-row cap, blank rows, frame glyphs and RGB verified; total cycles=%u frames=%u RGB VSYNC samples=%u with-visible-credits=%u\\n\",\n"
"                    clockticks6502 - startup_text_total_cycles,\n"
"                    shake_frames_seen - startup_text_total_frames, startup_text_frame_samples,\n"
"                    startup_text_visible_frame_samples);\n"
"                emu_write(5, 0);\n"
"                raise(SIGINT);\n"
"            }\n"
"        }\n"
"        return;\n"
"    }\n"
"    if (!startup_text_active && bank == 3 && pc == S__b3Display_v) {\n"
"        if (shake_read(S__currentLog) != 83) {\n"
"            fprintf(stderr, \"Startup display.v entered outside Logic83: currentLog=%u\\n\",\n"
"                    shake_read(S__currentLog));\n"
"            exit(3);\n"
"        }\n"
"        startup_text_row = shake_banked_read(shake_var_address(36), 0);\n"
"        startup_text_column = shake_banked_read(shake_var_address(37), 0);\n"
"        startup_text_message = shake_banked_read(shake_var_address(32), 0);\n"
"        if (startup_text_row == 18) {\n"
"            startup_text_phase++;\n"
"            startup_text_phase_call = 0;\n"
"            if (startup_text_phase == 1) {\n"
"                startup_text_total_cycles = clockticks6502;\n"
"                startup_text_total_frames = shake_frames_seen;\n"
"            }\n"
"            startup_text_phase_start_cycles = clockticks6502;\n"
"            startup_text_phase_start_frames = shake_frames_seen;\n"
"        }\n"
"        startup_text_init_published_state();\n"
"        if (!startup_text_phase || startup_text_phase > 23) {\n"
"            fprintf(stderr, \"Unexpected Logic83 startup phase %u at row=%u message=%u\\n\",\n"
"                    startup_text_phase, startup_text_row, startup_text_message);\n"
"            exit(3);\n"
"        }\n"
"        startup_text_return_pc = shake_call_return(&startup_text_return_sp);\n"
"        startup_text_entry_ram_bank = memory_get_ram_bank();\n"
"        startup_text_generation = combined_read16(S_irqCompletionGeneration, 6);\n"
"        startup_text_call_cycles = clockticks6502;\n"
"        startup_text_call_frames = shake_frames_seen;\n"
"        startup_text_active = true;\n"
"        return;\n"
"    }\n"
"    if (clockticks6502 > 500000000u) {\n"
"        fprintf(\n"
"            stderr,\n"
"            \"Timed out before natural KQ1 Logic83 startup credits completed: phases=%u calls=%u currentLog=%u PC=%04x bank=%u\\n\",\n"
"            startup_text_phase, startup_text_returns, shake_read(S__currentLog), pc, bank);\n"
"        exit(3);\n"
"    }\n"
"}\n"
"static void text_case_track_source_helpers(uint16_t pc) { while(text_source_depth && pc==text_source_frames[text_source_depth-1].return_pc && (uint8_t)regs.sp==text_source_frames[text_source_depth-1].return_sp) text_source_depth--; if(pc==S__strLenBankedBounded||pc==S__memCpyBankedBetween||pc==S__memCpyBanked) { if(text_source_depth>=sizeof(text_source_frames)/sizeof(text_source_frames[0])) { fprintf(stderr,\"Text source helper nesting exceeded its bound\\n\"); exit(3); } TextSourceFrame *frame=&text_source_frames[text_source_depth++]; frame->return_pc=shake_call_return(&frame->return_sp); } }\n"
"static void text_case_seed_map(void) { if(!text_case_active) { text_case_map_seeded=false; return; } for(unsigned row=3;row<=32;row++) for(unsigned column=0;column<64;column++) { uint32_t cell=0xda00u+row*128u+column*2u; video_space_write(cell,0x23); video_space_write(cell+1,0x23); } }\n"
"static void text_case_set_source(uint16_t source,uint8_t bank) { shake_ram(S__testBankAliasSource,(uint8_t)source); shake_ram(S__testBankAliasSource+1,(uint8_t)(source>>8)); shake_ram(S__testBankAliasBank,bank); text_case_source=source; text_case_source_bank=bank; }\n"
"static unsigned text_case_expected_char_writes(void) { switch(text_case_index) { case TEXT_CASE_BANK_ALIAS:return 40; case TEXT_CASE_BANK_BOUNDARY:return 4; case TEXT_CASE_FULL_CLEAR:return 29u*64u; case TEXT_CASE_BUFFER_END:return 1; case TEXT_CASE_OVERLAP:return 3u*40u; default:return 0; } }\n"
"static unsigned text_case_check_rows(void) { return text_case_index==TEXT_CASE_FULL_CLEAR?30:(text_case_index==TEXT_CASE_OVERLAP?4:2); }\n"
"static unsigned text_case_painted_rows(void) { return text_case_index==TEXT_CASE_FULL_CLEAR?29:(text_case_index==TEXT_CASE_OVERLAP?3:1); }\n"
"static uint8_t text_case_expected_char(unsigned row,unsigned column) { if(text_case_index==TEXT_CASE_FULL_CLEAR) return row<29?0x46:0x23; if(text_case_index==TEXT_CASE_BANK_ALIAS) return row==0&&column<40?'A':0x23; if(text_case_index==TEXT_CASE_BANK_BOUNDARY) return row==0&&column<4?(uint8_t)\"ABCD\"[column]:0x23; if(text_case_index==TEXT_CASE_BUFFER_END) return row==0&&column==0?'A':0x23; if(text_case_index==TEXT_CASE_OVERLAP) return row<3&&column<40?(uint8_t)('A'+row):0x23; return 0x23; }\n"
"static void text_case_check_map(void) { unsigned rows=text_case_check_rows(),painted=text_case_painted_rows(); uint32_t base=0xdb80u; for(unsigned row=0;row<rows;row++) for(unsigned column=0;column<64;column++) { uint32_t cell=base+row*128u+column*2u; uint8_t expected=text_case_expected_char(row,column),actual=video_space_read(cell),attribute=video_space_read(cell+1),expected_attribute=row<painted?0x10:0x23; if(actual!=expected||attribute!=expected_attribute) { fprintf(stderr,\"Text case VRAM mismatch: case=%u row=%u column=%u char=%02x expected=%02x attr=%02x expected_attr=%02x\\n\",text_case_index,row,column,actual,expected,attribute,expected_attribute); exit(3); } } }\n"
"static void text_case_start(unsigned index) { if(index>=TEXT_CASE_COUNT) { fprintf(stderr,\"Invalid text case index %u\\n\",index); exit(3); } text_case_index=index; text_case_active=false; text_case_source_trace=false; text_case_vram_trace=false; text_source_depth=0; text_case_source=0; text_case_source_bank=0; text_case_source_bytes=text_case_source_guard=0; text_case_source_reads=text_case_source_oob_reads=text_case_c000_reads=0; text_case_char_writes=text_case_last_write_cycle=text_case_generation_cycle=0; text_case_last_write_address=0; text_case_last_write_value=0; text_case_seed_map(); if(index==TEXT_CASE_BANK_ALIAS) { uint16_t source=S__b3TextBuffers; for(unsigned i=0;i<50;i++) { shake_banked_ram(source+i,0x24,'A'); shake_banked_ram(source+i,3,'Z'); } shake_banked_ram(source+50,0x24,0); shake_banked_ram(source+51,0x24,'!'); shake_banked_ram(source+50,3,0); text_case_set_source(source,0x24); text_case_source_bytes=51; text_case_source_guard=1; } else if(index==TEXT_CASE_BANK_BOUNDARY) { for(unsigned i=0;i<4;i++) shake_banked_ram((uint16_t)(0xbffc+i),0x24,(uint8_t)\"ABCD\"[i]); text_case_source=0xbffc; text_case_source_bank=0x24; text_case_source_bytes=4; text_case_source_guard=1; } else if(index==TEXT_CASE_BUFFER_END) { uint16_t source=(uint16_t)(S__b3TextBuffers+1999); shake_banked_ram(source,3,'A'); shake_banked_ram((uint16_t)(source+1),3,'B'); shake_banked_ram((uint16_t)(source+2),3,0); text_case_set_source(source,3); text_case_source_bytes=1; text_case_source_guard=2; } else if(index==TEXT_CASE_OVERLAP) { uint16_t source=(uint16_t)(S__b3TextBuffers+1); for(unsigned i=0;i<40;i++) shake_banked_ram(source+i,3,'A'); shake_banked_ram(source+40,3,10); for(unsigned i=0;i<40;i++) shake_banked_ram(source+41+i,3,'B'); shake_banked_ram(source+81,3,10); for(unsigned i=0;i<40;i++) shake_banked_ram(source+82+i,3,'C'); shake_banked_ram(source+122,3,0); shake_banked_ram(source+123,3,'!'); text_case_set_source(source,3); text_case_source_bytes=123; text_case_source_guard=1; } text_case_generation=combined_read16(S_irqCompletionGeneration,6); uint8_t command=shake_banked_read(S_sendIrqCommand,6),work=shake_banked_read(S_irqWorkKind,6),depth=shake_read(S__foregroundVeraDepth); if(command||work||depth||text_case_generation!=(uint16_t)(index?text_case_previous_generation:(combined_generation+1))) { fprintf(stderr,\"Text case started outside the completed KQ1 IRQ boundary: case=%u command/work=%u/%u depth=%u generation=%04x baseline=%04x\\n\",index,command,work,depth,text_case_generation,(uint16_t)(index?text_case_previous_generation:(combined_generation+1))); exit(3); } text_case_previous_generation=text_case_generation; uint16_t target=index==TEXT_CASE_FULL_CLEAR?S__testRunFullLayerClear:(index==TEXT_CASE_BANK_BOUNDARY?S__testRunBankBoundaryDisplay:S__testRunBankAliasDisplay); text_case_return_pc=regs.pc; text_case_return_sp=regs.sp; text_case_return_c_sp=combined_read16(S_c_sp,0); text_case_return_ram_bank=memory_get_ram_bank(); text_case_return_rom_bank=memory_get_rom_bank(); text_case_return_code_bank=regs.k; text_case_return_status=regs.status; if(!regs.e||(regs.sp&0xff00)!=0x0100||(regs.status&FLAG_INTERRUPT)||depth) { fprintf(stderr,\"Text case caller contract failed before target call: case=%u E=%u SP=%04x I=%u depth=%u\\n\",index,regs.e,regs.sp,(regs.status&FLAG_INTERRUPT)!=0,depth); exit(3); } text_case_active=true; text_case_source_trace=text_case_source_bytes!=0; text_case_vram_trace=true; shake_push_test_jsr(target); fprintf(stderr,\"Starting production text case %s (%u): source=%02x:%04x bytes=%u generation=%04x target=%04x\\n\",text_case_names[index],index,text_case_source_bank,text_case_source,text_case_source_bytes,text_case_generation,target); }\n"
"static void text_case_check_return(uint16_t pc) { if(!text_case_active||pc!=text_case_return_pc) return; uint8_t depth=shake_read(S__foregroundVeraDepth),command=shake_banked_read(S_sendIrqCommand,6),work=shake_banked_read(S_irqWorkKind,6); uint16_t generation=combined_read16(S_irqCompletionGeneration,6),c_sp=combined_read16(S_c_sp,0); unsigned expected_writes=text_case_expected_char_writes(); if(regs.sp!=text_case_return_sp||!regs.e||(regs.sp&0xff00)!=0x0100||c_sp!=text_case_return_c_sp||memory_get_ram_bank()!=text_case_return_ram_bank||memory_get_rom_bank()!=text_case_return_rom_bank||regs.k!=text_case_return_code_bank||((regs.status^text_case_return_status)&FLAG_INTERRUPT)||depth||command||work||generation!=(uint16_t)(text_case_generation+1)||!text_case_generation_cycle||!text_case_last_write_cycle||text_case_last_write_cycle>=text_case_generation_cycle||text_case_char_writes!=expected_writes||text_source_depth||(text_case_source_trace&&(text_case_source_reads!=2*text_case_source_bytes||text_case_source_oob_reads||((text_case_index==TEXT_CASE_BANK_BOUNDARY)&&text_case_c000_reads)))) { fprintf(stderr,\"Production text case return failed: %u (%s) SP=%04x/%04x Cstack=%04x/%04x banks=%u/%u=>%u/%u code=%u/%u I=%u/%u depth=%u command/work=%u/%u generation=%04x expected=%04x writes=%u/%u source=%u expected_source=%u oob=%u C000=%u helper_depth=%u last=%05x:%02x cycle=%u generation_cycle=%u\\n\",text_case_index,text_case_names[text_case_index],regs.sp,text_case_return_sp,c_sp,text_case_return_c_sp,text_case_return_ram_bank,text_case_return_rom_bank,memory_get_ram_bank(),memory_get_rom_bank(),regs.k,text_case_return_code_bank,(text_case_return_status&FLAG_INTERRUPT)!=0,(regs.status&FLAG_INTERRUPT)!=0,depth,command,work,generation,(uint16_t)(text_case_generation+1),text_case_char_writes,expected_writes,text_case_source_reads,2*text_case_source_bytes,text_case_source_oob_reads,text_case_c000_reads,text_source_depth,text_case_last_write_address,text_case_last_write_value,text_case_last_write_cycle,text_case_generation_cycle); exit(3); } text_case_check_map(); uint16_t expected_offset=(uint16_t)((expected_writes-1u)*2u); if(text_case_index==TEXT_CASE_FULL_CLEAR) expected_offset=(uint16_t)(28u*128u+126u); else if(text_case_index==TEXT_CASE_OVERLAP) expected_offset=(uint16_t)(2u*128u+78u); uint16_t expected_last=(uint16_t)(0xdb80u+expected_offset); if(text_case_last_write_address!=expected_last) { fprintf(stderr,\"Production text case ended at an unexpected VERA cell: case=%u last=%05x expected=%05x writes=%u\\n\",text_case_index,text_case_last_write_address,expected_last,text_case_char_writes); exit(3); } fprintf(stderr,\"Production text case %u (%s) passed: chars=%u source_reads=%u oob=%u C000=%u generation=%04x last_store=%05x@%u completion=%u\\n\",text_case_index,text_case_names[text_case_index],text_case_char_writes,text_case_source_reads,text_case_source_oob_reads,text_case_c000_reads,generation,text_case_last_write_address,text_case_last_write_cycle,text_case_generation_cycle); text_case_previous_generation=generation; text_case_active=false; text_case_vram_trace=false; text_case_source_trace=false; if(text_case_index+1<TEXT_CASE_COUNT) text_case_start(text_case_index+1); else { emu_write(5,0); raise(SIGINT); } }\n"
"static void shake_sprite_batch_capture_return(void) { uint8_t sp=(uint8_t)regs.sp; shake_sprite_batch_return_pc=(uint16_t)(shake_read(0x0100+(uint8_t)(sp+1))|((uint16_t)shake_read(0x0100+(uint8_t)(sp+2))<<8)); shake_sprite_batch_return_pc++; shake_sprite_batch_return_sp=(uint8_t)(sp+2); memset(shake_sprite_attr_written,0,sizeof(shake_sprite_attr_written)); shake_sprite_batch_handler_active=true; }\n"
"static void shake_capture_sprite_batch(void) {\n"
"    uint16_t base=S__bESpritesUpdatedBuffer, end=combined_read16(S__bESpritesUpdatedBufferPointer,14); shake_sprite_batch_handler_entries++; shake_sprite_batch_last_pointer=end; shake_sprite_batch_last_first=shake_banked_read(base,14); shake_sprite_batch_last_second=shake_banked_read(base+1,14); if(end<base) { fprintf(stderr,\"Game sprite producer pointer precedes its buffer: base=%04x end=%04x\\n\",base,end); exit(3); } if(end==base) { shake_sprite_batch_empty_entries++; return; }\n"
"    if((uint32_t)end>(uint32_t)base+SHAKE_SPRITE_BUFFER_SIZE) { fprintf(stderr,\"Game sprite producer pointer is outside its buffer: base=%04x end=%04x\\n\",base,end); exit(3); }\n"
"    unsigned records=0;\n"
"    while(records<SHAKE_SPRITE_UPDATE_MAX_RECORDS) { uint16_t record=(uint16_t)(base+records*SHAKE_SPRITE_UPDATE_RECORD_BYTES); if(!(shake_banked_read(record,14)|shake_banked_read(record+1,14))) break; records++; }\n"
"    uint16_t terminator=(uint16_t)(base+records*SHAKE_SPRITE_UPDATE_RECORD_BYTES);\n"
"    if(shake_banked_read(terminator,14)||shake_banked_read(terminator+1,14)) { fprintf(stderr,\"Game sprite batch has no bounded terminator: base=%04x producer=%04x records=%u\\n\",base,end,records); exit(3); }\n"
"    if(!records) { for(uint16_t i=(uint16_t)(base+SHAKE_SPRITE_TERMINATOR_BYTES);i<end;i++) if(shake_banked_read(i,14)) { fprintf(stderr,\"Game sprite producer published data behind an earlier empty terminator: base=%04x end=%04x hidden=%04x value=%02x\\n\",base,end,i,shake_banked_read(i,14)); exit(3); } shake_sprite_batch_empty_entries++; return; }\n"
"    if(end<(uint16_t)(terminator+SHAKE_SPRITE_TERMINATOR_BYTES)) { fprintf(stderr,\"Game sprite producer pointer precedes its terminator: base=%04x producer=%04x records=%u\\n\",base,end,records); exit(3); }\n"
"    for(uint16_t i=(uint16_t)(terminator+SHAKE_SPRITE_TERMINATOR_BYTES);i<end;i++) if(shake_banked_read(i,14)) { fprintf(stderr,\"Game sprite publication has data inside its published range after the first terminator: base=%04x end=%04x records=%u terminator=%04x trailing=%u hidden=%04x value=%02x warm_b9_total=%u since_handler=%u last_batch=%u\\n\",base,end,records,terminator,(unsigned)(end-(terminator+SHAKE_SPRITE_TERMINATOR_BYTES)),i,shake_banked_read(i,14),shake_view_warm_total_b9_entries,shake_view_warm_b9_since_handler,shake_view_warm_last_batch_b9_entries); for(unsigned j=shake_view_warm_last_batch_start;j<shake_view_warm_call_count;j++) { WarmSpriteProducerCall *call=&shake_view_warm_calls[j]; fprintf(stderr,\"  b9[%u] cycles=%u..%u pointer=%04x->%04x ownerDepth=%u->%u completed=%u return=%04x/%02x\\n\",j,call->entry_cycle,call->exit_cycle,call->entry_pointer,call->exit_pointer,call->entry_depth,call->exit_depth,call->complete,call->return_pc,call->return_sp); } exit(3); }\n"
"    shake_sprite_batch_length=(uint16_t)(records*SHAKE_SPRITE_UPDATE_RECORD_BYTES+SHAKE_SPRITE_TERMINATOR_BYTES);\n"
"    for(unsigned i=0;i<shake_sprite_batch_length;i++) shake_saved_sprite_batch[i]=shake_banked_read(base+i,14);\n"
"    shake_sprite_batch_saved=true; shake_sprite_batch_replay_active=false; shake_sprite_batch_warm_latest=false; if(shake_view_warm_active&&shake_view_warm_latest_snapshot_saved) { if(shake_sprite_batch_length!=shake_view_warm_latest_snapshot_length||memcmp(shake_saved_sprite_batch,shake_view_warm_latest_snapshot,shake_sprite_batch_length)) { fprintf(stderr,\"Sprite IRQ input differs from the latest real producer snapshot: input=%u latest=%u\\n\",shake_sprite_batch_length,shake_view_warm_latest_snapshot_length); exit(3); } shake_sprite_batch_warm_latest=true; shake_view_warm_irq_input_latest=true; } shake_sprite_batch_capture_return();\n"
"}\n"
"static unsigned shake_copy_warm_sprite_snapshot(uint8_t *out,uint16_t *out_length) { uint16_t base=S__bESpritesUpdatedBuffer,end=combined_read16(S__bESpritesUpdatedBufferPointer,14); if(end<base||(uint32_t)end>(uint32_t)base+SHAKE_SPRITE_BUFFER_SIZE) { fprintf(stderr,\"Warm sprite publication pointer outside buffer: base=%04x end=%04x\\n\",base,end); exit(3); } unsigned records=0; while(records<SHAKE_SPRITE_UPDATE_MAX_RECORDS) { uint16_t record=(uint16_t)(base+records*SHAKE_SPRITE_UPDATE_RECORD_BYTES); if(!(shake_banked_read(record,14)|shake_banked_read(record+1,14))) break; records++; } uint16_t terminator=(uint16_t)(base+records*SHAKE_SPRITE_UPDATE_RECORD_BYTES),expected=(uint16_t)(terminator+SHAKE_SPRITE_TERMINATOR_BYTES); if(shake_banked_read(terminator,14)||shake_banked_read(terminator+1,14)) { fprintf(stderr,\"Warm sprite publication has no bounded terminator: base=%04x end=%04x records=%u\\n\",base,end,records); exit(3); } if(end!=expected) { for(uint16_t i=expected;i<end;i++) if(shake_banked_read(i,14)) { fprintf(stderr,\"Warm sprite publication contains records after its first terminator: base=%04x end=%04x records=%u terminator=%04x hidden=%04x value=%02x\\n\",base,end,records,terminator,i,shake_banked_read(i,14)); exit(3); } fprintf(stderr,\"Warm sprite publication pointer does not end at its terminator: base=%04x end=%04x expected=%04x records=%u\\n\",base,end,expected,records); exit(3); } if(!records) { fprintf(stderr,\"KQ1 warm producer published no visible sprite records\\n\"); exit(3); } *out_length=(uint16_t)(expected-base); for(unsigned i=0;i<*out_length;i++) out[i]=shake_banked_read(base+i,14); return records; }\n"
"static bool shake_warm_coordinates_differ(const uint8_t *first,uint16_t first_length,const uint8_t *latest,uint16_t latest_length) { if(first_length!=latest_length) return false; unsigned records=(first_length-SHAKE_SPRITE_TERMINATOR_BYTES)/SHAKE_SPRITE_UPDATE_RECORD_BYTES; for(unsigned i=0;i<records;i++) { const uint8_t *a=first+i*SHAKE_SPRITE_UPDATE_RECORD_BYTES,*b=latest+i*SHAKE_SPRITE_UPDATE_RECORD_BYTES; if(a[2]!=b[2]||a[3]!=b[3]||a[4]!=b[4]) return true; } return false; }\n"
"static void shake_record_warm_sprite_publication(WarmSpriteProducerCall *call) { if(!shake_view_warm_active||!shake_view_warm_position_entries) return; uint8_t snapshot[SHAKE_SPRITE_BUFFER_SIZE]; uint16_t length=0; unsigned records=shake_copy_warm_sprite_snapshot(snapshot,&length); if(shake_view_warm_position_entries==1&&shake_view_warm_draw_entries>=1&&!shake_view_warm_first_snapshot_saved) { memcpy(shake_view_warm_first_snapshot,snapshot,length); shake_view_warm_first_snapshot_length=length; shake_view_warm_first_snapshot_saved=true; fprintf(stderr,\"Warm real sprite publication 1: pointer=%04x->%04x records=%u\\n\",call->entry_pointer,call->exit_pointer,records); } if(shake_view_warm_position_entries>=2) { if(shake_view_warm_first_snapshot_saved) { unsigned first_records=(shake_view_warm_first_snapshot_length-SHAKE_SPRITE_TERMINATOR_BYTES)/SHAKE_SPRITE_UPDATE_RECORD_BYTES; if(first_records!=records) { fprintf(stderr,\"Warm sprite snapshot record count changed across controlled position update: first=%u latest=%u\\n\",first_records,records); exit(3); } } if(!shake_view_warm_latest_snapshot_saved) { shake_view_warm_second_entry_pending=call->entry_pointer!=S__bESpritesUpdatedBuffer; if(!shake_view_warm_second_entry_pending) { fprintf(stderr,\"Second real position/draw publication began after the prior sprite snapshot was consumed: entry=%04x base=%04x\\n\",call->entry_pointer,S__bESpritesUpdatedBuffer); exit(3); } } memcpy(shake_view_warm_latest_snapshot,snapshot,length); shake_view_warm_latest_snapshot_length=length; shake_view_warm_latest_snapshot_saved=true; if(shake_view_warm_first_snapshot_saved&&shake_warm_coordinates_differ(shake_view_warm_first_snapshot,shake_view_warm_first_snapshot_length,shake_view_warm_latest_snapshot,shake_view_warm_latest_snapshot_length)) shake_view_warm_coordinates_changed=true; } }\n"
"static void shake_verify_warm_sprite_coordinates(void) { unsigned records=(shake_view_warm_latest_snapshot_length-SHAKE_SPRITE_TERMINATOR_BYTES)/SHAKE_SPRITE_UPDATE_RECORD_BYTES; for(unsigned i=0;i<records;i++) { const uint8_t *record=shake_view_warm_latest_snapshot+i*SHAKE_SPRITE_UPDATE_RECORD_BYTES; unsigned attr=8+i*8; for(unsigned coordinate=0;coordinate<3;coordinate++) { unsigned offset=attr+2+coordinate; uint8_t expected=record[2+coordinate],actual=video_space_read(0x1fc00+offset); if(!shake_sprite_attr_written[offset]||shake_sprite_attr_actual[offset]!=expected||actual!=expected) { fprintf(stderr,\"Sprite IRQ did not commit latest producer coordinate to VERA: sprite=%u component=%u expected=%02x observed_write=%02x VERA=%02x written=%u\\n\",i+1,coordinate,expected,shake_sprite_attr_actual[offset],actual,shake_sprite_attr_written[offset]); exit(3); } } } }\n"
"static void shake_validate_live_sprite_batch(void) { uint16_t base=S__bESpritesUpdatedBuffer,end=combined_read16(S__bESpritesUpdatedBufferPointer,14); if(end==base) return; if(end<base||(uint32_t)end>(uint32_t)base+SHAKE_SPRITE_BUFFER_SIZE) { fprintf(stderr,\"Live sprite producer pointer outside buffer before replay: base=%04x end=%04x\\n\",base,end); exit(3); } unsigned records=0; while(records<SHAKE_SPRITE_UPDATE_MAX_RECORDS) { uint16_t record=(uint16_t)(base+records*SHAKE_SPRITE_UPDATE_RECORD_BYTES); if(!(shake_banked_read(record,14)|shake_banked_read(record+1,14))) break; records++; } uint16_t terminator=(uint16_t)(base+records*SHAKE_SPRITE_UPDATE_RECORD_BYTES),expected=(uint16_t)(terminator+SHAKE_SPRITE_TERMINATOR_BYTES); if(shake_banked_read(terminator,14)||shake_banked_read(terminator+1,14)) { fprintf(stderr,\"Live sprite producer has no bounded terminator before replay: base=%04x end=%04x records=%u\\n\",base,end,records); exit(3); } if(end!=expected) { fprintf(stderr,\"Live sprite producer pointer does not end at its first terminator before replay: base=%04x end=%04x expected=%04x records=%u\\n\",base,end,expected,records); exit(3); } }\n"
"static void shake_begin_sprite_batch_replay(void) { uint16_t base=S__bESpritesUpdatedBuffer, expected=(uint16_t)(base+shake_sprite_batch_length); shake_validate_live_sprite_batch(); for(unsigned i=0;i<shake_sprite_batch_length;i++) shake_banked_ram(base+i,14,shake_saved_sprite_batch[i]); shake_banked_ram(S__bESpritesUpdatedBufferPointer,14,(uint8_t)expected); shake_banked_ram(S__bESpritesUpdatedBufferPointer+1,14,(uint8_t)(expected>>8)); uint16_t pointer=combined_read16(S__bESpritesUpdatedBufferPointer,14); if(pointer!=expected) { fprintf(stderr,\"Replayed sprite batch pointer mismatch: pointer=%04x expected=%04x\\n\",pointer,expected); exit(3); } for(unsigned i=0;i<shake_sprite_batch_length;i++) if(shake_banked_read(base+i,14)!=shake_saved_sprite_batch[i]) { fprintf(stderr,\"Replayed sprite batch changed before production handler at byte %u\\n\",i); exit(3); } shake_sprite_batch_capture_return(); }\n"
"static void shake_check_sprite_batch_handler(uint16_t pc,uint8_t bank) { if(shake_sprite_batch_handler_active&&pc==shake_sprite_batch_return_pc&&(uint8_t)regs.sp==shake_sprite_batch_return_sp) { uint16_t pointer=combined_read16(S__bESpritesUpdatedBufferPointer,14); if(pointer!=S__bESpritesUpdatedBuffer) { fprintf(stderr,\"Production sprite IRQ did not consume the complete batch: pointer=%04x base=%04x\\n\",pointer,S__bESpritesUpdatedBuffer); exit(3); } if(shake_sprite_batch_replay_active) { shake_sprite_batch_replayed=true; shake_sprite_batch_replay_requested=false; } else { shake_sprite_batch_processed=true; if(shake_sprite_batch_warm_latest) { shake_verify_warm_sprite_coordinates(); shake_view_warm_second_batch_consumed=true; } } shake_sprite_batch_handler_active=false; shake_sprite_batch_replay_active=false; } (void)bank; }\n"
"static uint32_t combined_vsync_counter(void) { return (uint32_t)shake_banked_read(S__vSyncCounter,6)|((uint32_t)shake_banked_read(S__vSyncCounter+1,6)<<8)|((uint32_t)shake_banked_read(S__vSyncCounterHigh,6)<<16)|((uint32_t)shake_banked_read(S__vSyncCounterHigh+1,6)<<24); }\n"
"static unsigned combined_count_sprite_records(void) { uint16_t base=S__bESpritesUpdatedBuffer,end=combined_read16(S__bESpritesUpdatedBufferPointer,14); if(end==base) return 0; if(end<base||(uint32_t)end>(uint32_t)base+SHAKE_SPRITE_BUFFER_SIZE) { fprintf(stderr,\"Combined producer pointer outside sprite buffer: base=%04x end=%04x\\n\",base,end); exit(3); } for(unsigned records=0;records<=SHAKE_SPRITE_UPDATE_MAX_RECORDS;records++) { uint16_t record=(uint16_t)(base+records*SHAKE_SPRITE_UPDATE_RECORD_BYTES); if((uint32_t)record+SHAKE_SPRITE_TERMINATOR_BYTES>end) { fprintf(stderr,\"Combined sprite publication ended before a bounded terminator: base=%04x end=%04x records=%u\\n\",base,end,records); exit(3); } if(!(shake_banked_read(record,14)|shake_banked_read(record+1,14))) { uint16_t expected=(uint16_t)(record+SHAKE_SPRITE_TERMINATOR_BYTES); if(end!=expected) { fprintf(stderr,\"Combined sprite pointer does not end at its first terminator: base=%04x end=%04x expected=%04x records=%u\\n\",base,end,expected,records); exit(3); } return records; } if(records==SHAKE_SPRITE_UPDATE_MAX_RECORDS||(uint32_t)record+SHAKE_SPRITE_UPDATE_RECORD_BYTES>end) { fprintf(stderr,\"Combined sprite publication exceeds its record bound or omits its terminator: base=%04x end=%04x records=%u\\n\",base,end,records); exit(3); } } fprintf(stderr,\"Combined sprite publication has no bounded terminator\\n\"); exit(3); }\n"
"static unsigned combined_stage_latest_sprite_snapshot(void) { uint16_t base=S__bESpritesUpdatedBuffer,current=combined_read16(S__bESpritesUpdatedBufferPointer,14),length=shake_view_warm_latest_snapshot_length; shake_validate_live_sprite_batch(); if(!shake_view_warm_second_batch_consumed||!shake_view_warm_latest_snapshot_saved||!shake_view_warm_coordinates_changed||length<SHAKE_SPRITE_TERMINATOR_BYTES||(length-SHAKE_SPRITE_TERMINATOR_BYTES)%SHAKE_SPRITE_UPDATE_RECORD_BYTES||current!=base) { fprintf(stderr,\"Cannot replay the authentic latest KQ1 sprite snapshot: consumed=%u saved=%u changed=%u length=%u pointer=%04x base=%04x\\n\",shake_view_warm_second_batch_consumed,shake_view_warm_latest_snapshot_saved,shake_view_warm_coordinates_changed,length,current,base); exit(3); } unsigned records=(length-SHAKE_SPRITE_TERMINATOR_BYTES)/SHAKE_SPRITE_UPDATE_RECORD_BYTES; if(!records||records>SHAKE_SPRITE_UPDATE_MAX_RECORDS||shake_view_warm_latest_snapshot[length-SHAKE_SPRITE_TERMINATOR_BYTES]||shake_view_warm_latest_snapshot[length-SHAKE_SPRITE_TERMINATOR_BYTES+1]) { fprintf(stderr,\"Captured KQ1 sprite snapshot has an invalid bounded record count or terminator: records=%u length=%u\\n\",records,length); exit(3); } for(unsigned i=0;i<length;i++) shake_banked_ram(base+i,14,shake_view_warm_latest_snapshot[i]); uint16_t end=(uint16_t)(base+length); shake_banked_ram(S__bESpritesUpdatedBufferPointer,14,(uint8_t)end); shake_banked_ram(S__bESpritesUpdatedBufferPointer+1,14,(uint8_t)(end>>8)); shake_validate_live_sprite_batch(); unsigned observed=combined_count_sprite_records(); if(observed!=records) { fprintf(stderr,\"Replayed KQ1 sprite record count changed during bounded publication: captured=%u live=%u\\n\",records,observed); exit(3); } fprintf(stderr,\"Staged the exact latest current-run KQ1 sprite snapshot for the mixed IRQ row: records=%u bytes=%u terminator=%02x%02x\\n\",records,length,shake_banked_read(end-SHAKE_SPRITE_TERMINATOR_BYTES,14),shake_banked_read(end-1,14)); return records; }\n"
"static void combined_capture_call(uint16_t *return_pc,uint8_t *return_sp) { uint8_t sp=(uint8_t)regs.sp; uint16_t ret=(uint16_t)(shake_read(0x0100+(uint8_t)(sp+1))|((uint16_t)shake_read(0x0100+(uint8_t)(sp+2))<<8)); *return_pc=(uint16_t)(ret+1); *return_sp=(uint8_t)(sp+2); }\n"
"static void combined_call_text_publisher(uint16_t target) { uint8_t depth=shake_read(S__foregroundVeraDepth); if(combined_text_publisher_active||depth||(regs.status&FLAG_INTERRUPT)||!regs.e||(regs.sp&0xff00)!=0x0100) { fprintf(stderr,\"Blocking text publisher entry contract failed: active=%u depth=%u I=%u E=%u SP=%04x\\n\",combined_text_publisher_active,depth,(regs.status&FLAG_INTERRUPT)!=0,regs.e,regs.sp); exit(3); } combined_text_publisher_return_pc=regs.pc; combined_text_publisher_return_sp=regs.sp; combined_text_publisher_c_sp=combined_read16(S_c_sp,0); combined_text_publisher_ram_bank=memory_get_ram_bank(); combined_text_publisher_rom_bank=memory_get_rom_bank(); combined_text_publisher_code_bank=regs.k; combined_text_publisher_status=regs.status; combined_text_publisher_active=true; shake_push_test_jsr(target); fprintf(stderr,\"Called compiled blocking display-text publisher: target=%04x source=%04x destination=%05x entrySP=%04x Cstack=%04x I=0\\n\",target,combined_text_buffer,combined_text_destination,combined_text_publisher_return_sp,combined_text_publisher_c_sp); }\n"
"static bool combined_text_attributes_complete(void) {\n"
"    uint8_t expected = (uint8_t)(shake_banked_read(S__b3PaletteNumber, 3) << 4);\n"
"    if (combined_text_attribute_pending || combined_text_attribute_writes != 513 ||\n"
"        combined_text_last_attribute_write_address !=\n"
"            (uint16_t)(combined_text_destination + 2 * 512 + 1) ||\n"
"        !combined_generation_cycle ||\n"
"        combined_text_last_attribute_write_cycle >= combined_generation_cycle) {\n"
"        fprintf(\n"
"            stderr,\n"
"            \"Combined text attributes did not all precede completion: count=%u pending=%u last=%05x expected-last=%05x cycle=%u completion-cycle=%u palette=%02x\\n\",\n"
"            combined_text_attribute_writes, combined_text_attribute_pending,\n"
"            combined_text_last_attribute_write_address,\n"
"            (uint16_t)(combined_text_destination + 2 * 512 + 1),\n"
"            combined_text_last_attribute_write_cycle, combined_generation_cycle, expected);\n"
"        return false;\n"
"    }\n"
"    for (unsigned i = 0; i < 513; i++) {\n"
"        uint32_t address = (uint32_t)combined_text_destination + 2 * i + 1;\n"
"        uint8_t actual = video_space_read(address);\n"
"        if (actual != expected) {\n"
"            fprintf(\n"
"                stderr,\n"
"                \"Combined text attribute changed after publication: cell=%u address=%05x got=%02x expected=%02x\\n\",\n"
"                i, address, actual, expected);\n"
"            return false;\n"
"        }\n"
"    }\n"
"    return true;\n"
"}\n"
"static void combined_check_text_publisher_return(uint16_t pc) { if(!combined_text_publisher_active||pc!=combined_text_publisher_return_pc) return; combined_check_multiply_results(); uint8_t depth=shake_read(S__foregroundVeraDepth),command=shake_banked_read(S_sendIrqCommand,6),work=shake_banked_read(S_irqWorkKind,6); uint16_t generation=combined_read16(S_irqCompletionGeneration,6); uint16_t c_sp=combined_read16(S_c_sp,0); if(regs.sp!=combined_text_publisher_return_sp||!regs.e||(regs.sp&0xff00)!=0x0100||c_sp!=combined_text_publisher_c_sp||memory_get_ram_bank()!=combined_text_publisher_ram_bank||memory_get_rom_bank()!=combined_text_publisher_rom_bank||regs.k!=combined_text_publisher_code_bank||((regs.status^combined_text_publisher_status)&FLAG_INTERRUPT)||depth||!combined_complete||combined_row_count!=2||combined_irq_entry_count!=2||command||work||generation!=(uint16_t)(combined_generation+1)||combined_text_total_writes!=513||combined_rows[0].text_vram_writes!=512||combined_rows[1].text_vram_writes!=1||combined_text_last_write_address!=(uint16_t)(combined_text_destination+2*512)||combined_text_last_write_value!='K'||!combined_generation_cycle||combined_text_last_write_cycle>=combined_generation_cycle||shake_banked_read(combined_text_buffer+513,3)!=0||!combined_guard_seeded||!combined_guard_menu_clear_seen||combined_guard_post_seed_writes) { fprintf(stderr,\"Blocking display-text return contract failed: SP=%04x/%04x Cstack=%04x/%04x banks=%u/%u=>%u/%u codebank=%u=>%u I=%u=>%u depth=%u complete=%u events=%u rows=%u command/work=%u/%u generation=%04x expected=%04x writes=%u rows=%u/%u last=%05x:%02x cycle=%u generation_cycle=%u terminator=%02x\\n\",regs.sp,combined_text_publisher_return_sp,c_sp,combined_text_publisher_c_sp,combined_text_publisher_ram_bank,combined_text_publisher_rom_bank,memory_get_ram_bank(),memory_get_rom_bank(),combined_text_publisher_code_bank,regs.k,(combined_text_publisher_status&FLAG_INTERRUPT)!=0,(regs.status&FLAG_INTERRUPT)!=0,depth,combined_complete,combined_irq_entry_count,combined_row_count,command,work,generation,(uint16_t)(combined_generation+1),combined_text_total_writes,combined_rows[0].text_vram_writes,combined_rows[1].text_vram_writes,combined_text_last_write_address,combined_text_last_write_value,combined_text_last_write_cycle,combined_generation_cycle,shake_banked_read(combined_text_buffer+513,3)); exit(3); } for(unsigned i=0;i<513;i++) if(video_space_read(combined_text_destination+2*i)!='K') { fprintf(stderr,\"Blocking display-text publisher left VRAM cell %u at %05x=%02x, expected K\\n\",i,combined_text_destination+2*i,video_space_read(combined_text_destination+2*i)); exit(3); } if(video_space_read(combined_text_destination+2*513)!=0x23) { fprintf(stderr,\"Blocking display-text publisher overwrote its guarded cell: %05x=%02x\\n\",combined_text_destination+2*513,video_space_read(combined_text_destination+2*513)); exit(3); } fprintf(stderr,\"Blocking display-text publisher returned after two production IRQ slices: text writes=512+1, generation=%04x, last VERA write cycle=%u before completion cycle=%u, completion=%u VSYNCs\\n\",generation,combined_text_last_write_cycle,combined_generation_cycle,combined_completion_frames); combined_text_publisher_active=false; text_case_start(TEXT_CASE_BUFFER_END); }\n"
"static void combined_write_csv(void) { fprintf(combined_csv,\"row,cycles,i_masked_cycles,calls,sound_channels,psg_writes,audible_psg_writes,menu_count,menu_vram_writes,sprite_records,sprite_vram_writes,text_vram_writes,command,work_kind,total_irq_cycles,max_i_masked_cycles,completion_frames,asserted_frames,serviced_frames,entry_depth,entry_i\\n\"); for(unsigned i=0;i<combined_row_count;i++) { CombinedIRQRow *r=&combined_rows[i]; fprintf(combined_csv,\"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\\n\",i+1,r->cycles,r->i_masked_cycles,r->calls,r->sound_channels,r->psg_writes,r->audible_psg_writes,r->menu_count,r->menu_vram_writes,r->sprite_records,r->sprite_vram_writes,r->text_vram_writes,r->command,r->work_kind,combined_total_cycles,combined_max_i_masked_cycles,combined_completion_frames,combined_asserted_frames,combined_serviced_frames,r->entry_depth,r->entry_i); } if(fflush(combined_csv)||fclose(combined_csv)) { fprintf(stderr,\"Could not write combined IRQ samples\\n\"); exit(3); } combined_csv=NULL; }\n"
"static void combined_finish_row(void) { if(combined_row_count>=2||!combined_mask_end||!combined_i_contiguous||!combined_i_pending_boundary_seen||combined_mask_start!=combined_accept_cycles) { fprintf(stderr,\"Combined IRQ did not have one acceptance-to-RTI I-masked interval (start=%u accepted=%u pending_boundary=%u contiguous=%u end=%u)\\n\",combined_mask_start,combined_accept_cycles,combined_i_pending_boundary_seen,combined_i_contiguous,combined_mask_end); exit(3); } uint32_t cycles=clockticks6502-combined_row_start_cycles,masked=combined_mask_end-combined_mask_start; CombinedIRQRow *r=&combined_rows[combined_row_count++]; r->cycles=cycles; r->i_masked_cycles=masked; r->calls=combined_row_calls; r->sound_channels=combined_sound_channels; r->psg_writes=combined_psg_writes; r->audible_psg_writes=combined_audible_psg_writes; r->menu_count=combined_row_count==1?combined_menu_count:0; r->menu_vram_writes=combined_menu_vram_writes; r->sprite_records=combined_row_count==1?combined_sprite_records:0; r->sprite_vram_writes=combined_sprite_vram_writes; r->text_vram_writes=combined_text_vram_writes; r->command=combined_row_command; r->work_kind=combined_row_work_kind; r->entry_depth=combined_irq_entry_depth; r->entry_i=combined_irq_entry_i; combined_total_cycles+=cycles; if(cycles>combined_max_cycles) combined_max_cycles=cycles; if(masked>combined_max_i_masked_cycles) combined_max_i_masked_cycles=masked; combined_irq_active=false; combined_sound_active=combined_menu_active=combined_sprite_active=combined_text_active=false; }\n"
"static void combined_before_step(uint16_t pc,uint8_t bank) {\n"
"    if(!waiting&&(combined_irq_pending||combined_irq_active)) { if(combined_irq_pending) combined_i_pending_boundary_seen=true; if(!(regs.status&FLAG_INTERRUPT)) combined_i_contiguous=false; }\n"
"    if(regs.k||waiting) return;\n"
"    if(text_case_active&&!text_case_map_seeded) { text_case_vram_trace=false; if(bank==3&&pc==S_handleDisplayTextSlice) { text_case_seed_map(); text_case_map_seeded=true; text_case_vram_trace=true; fprintf(stderr,\"Seeded text geometry map after real input/menu IRQ work: case=%u PC=%04x bank=%02x cycle=%u\\n\",text_case_index,pc,bank,clockticks6502); } }\n"
"    if(text_case_active&&pc==text_case_return_pc) { text_case_check_return(pc); return; }\n"
"    if(combined_text_publisher_active&&combined_complete&&pc==combined_text_publisher_return_pc) { combined_guard_trace_active=false; combined_irq_active=false; combined_sound_active=combined_menu_active=combined_sprite_active=combined_text_active=false; combined_check_text_publisher_return(pc); return; }\n"
"    if(!combined_start_return_queued&&clockticks6502>=TRIGGER_CYCLE) { static char combined_start_return[]=\"\\r\"; machine_paste(combined_start_return,false); combined_start_return_queued=true; shake_start_return_queued=true; fprintf(stderr,\"Queued a real KQ1 Return at cycle %u for combined setup\\n\",clockticks6502); }\n"
"    if(!combined_injected) {\n"
"        if(clockticks6502>TRIGGER_CYCLE+50000000u) { uint16_t menus=(uint16_t)(shake_read(S__numOfMenus)|((uint16_t)shake_read(S__numOfMenus+1)<<8)); unsigned sprites=combined_count_sprite_records(); uint8_t channels=shake_banked_read(S__bBChannelsPlaying,11),charset=shake_banked_read(S__charSetInited,3),room=shake_banked_read(shake_var_address(0),0),key=shake_banked_read(shake_var_address(19),0),start_flag=shake_banked_read(shake_flag_address(9),0),sp=(uint8_t)regs.sp; uint16_t ret0=(uint16_t)(shake_read(0x0100+(uint8_t)(sp+1))|((uint16_t)shake_read(0x0100+(uint8_t)(sp+2))<<8)),ret1=(uint16_t)(shake_read(0x0100+(uint8_t)(sp+3))|((uint16_t)shake_read(0x0100+(uint8_t)(sp+4))<<8)); fprintf(stderr,\"No combined KQ1 IRQ setup reached active sound/menu/sprite work: channels=%u menus=%u sprites=%u charset=%u room=%u var19=%02x flag9=%u input=%u pc=%04x bank=%u regs=%02x/%02x/%02x/%02x sp=%02x ret=%04x/%04x main=%04x loop=%u warm=%u/%u/%u cursor=%u:%u code=%04x end=%04x commands=%u/%u/%u/%u/%u sound=%u menu=%u capture=%u wait=%u k=%u calls=%u/%u/%u/%u/%u mainCycles=%u..%u\\n\",channels,menus,sprites,charset,room,key,start_flag,shake_read(S__inputLineDisplayed),pc,bank,regs.a,regs.x,regs.y,regs.status,sp,ret0,ret1,S_mainLoop,shake_read(S__mainLoopCounter),shake_view_warm_started,shake_view_warm_active,shake_view_warm_finished,shake_read(S_cwCurrentCode+1),shake_read(S_cwCurrentCode),(unsigned)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8)),(unsigned)(shake_read(S_endPos)|((uint16_t)shake_read(S_endPos+1)<<8)),shake_view_warm_load_entries,shake_view_warm_animate_entries,shake_view_warm_set_entries,shake_view_warm_position_entries,shake_view_warm_draw_entries,shake_banked_read(S__bBChannelsPlaying,11),menus,shake_capture_started,waiting,regs.k,combined_interpret_hits,combined_logic_hits,combined_mainloop_hits,combined_logic_entry_hits,combined_poll_hits,combined_first_mainloop_cycle,combined_last_mainloop_cycle); exit(3); }\n"
"        if(!shake_view_warm_finished) return;\n"
"        if(pc!=S_mainLoop||clockticks6502<TRIGGER_CYCLE||(regs.status&FLAG_INTERRUPT)) return;\n"
"        if(shake_banked_read(S_sendIrqCommand,6)||shake_banked_read(S_irqWorkKind,6)||!shake_banked_read(S__charSetInited,3)||!shake_banked_read(S__bBChannelsPlaying,11)) return;\n"
"        uint16_t sound0_pointer=combined_read16(S__bBLoadedSoundsPointer,11); uint8_t sound_end_flag=shake_banked_read(S__bBEndSoundFlag,11),sound_enabled=shake_banked_read(shake_flag_address(9),0),sound_end_flag_value=shake_banked_read(shake_flag_address(sound_end_flag),0); if(sound0_pointer<0xa000||sound0_pointer>=0xc000||!sound_enabled||sound_end_flag_value) { fprintf(stderr,\"KQ1 SOUND0 restart precondition failed: loaded_pointer=%04x flag9=%u flag[%u]=%u live_end_flag=%u\\n\",sound0_pointer,sound_enabled,sound_end_flag,sound_end_flag_value,sound_end_flag); exit(3); } shake_ram(S__testCombinedSoundEndFlag,sound_end_flag); fprintf(stderr,\"Restarting loaded KQ1 SOUND0 through bBPlaySound with its live end flag %u before the blocking text publish\\n\",sound_end_flag);\n"
"        combined_menu_count=(uint16_t)(shake_read(S__numOfMenus)|((uint16_t)shake_read(S__numOfMenus+1)<<8)); if(shake_view_warm_latest_snapshot_length<SHAKE_SPRITE_TERMINATOR_BYTES) { fprintf(stderr,\"KQ1 warm-up did not capture a bounded latest sprite snapshot\\n\"); exit(3); } combined_sprite_records=(shake_view_warm_latest_snapshot_length-SHAKE_SPRITE_TERMINATOR_BYTES)/SHAKE_SPRITE_UPDATE_RECORD_BYTES;\n"
"        if(!combined_menu_count||!combined_sprite_records) return;\n"
"        combined_text_buffer=combined_read16(S__currentTextBuffer,3); combined_text_destination=0xDC00; if(combined_text_buffer<0xA000||combined_text_buffer>0xBDFE) { fprintf(stderr,\"KQ1 text buffer is outside bank 3\\n\"); exit(3); }\n"
"        combined_csv=fopen(COMBINED_CSV,\"w\"); if(!combined_csv) { perror(COMBINED_CSV); exit(2); }\n"
"        combined_inject_frames=shake_frames_seen; combined_inject_counter=combined_vsync_counter(); combined_generation=combined_read16(S_irqCompletionGeneration,6); combined_text_total_writes=combined_text_attribute_writes=combined_text_last_write_cycle=combined_text_last_attribute_write_cycle=combined_generation_cycle=0; combined_text_last_write_address=combined_text_last_attribute_write_address=combined_text_pending_attribute_address=0; combined_text_last_write_value=combined_text_pending_attribute_value=0; combined_text_attribute_pending=false;\n"
"        for(unsigned i=0;i<514;i++) shake_banked_ram(combined_text_buffer+i,3,i==513?0:'K');\n"
"        shake_banked_ram(S__currentTextBuffer,3,(uint8_t)combined_text_buffer); shake_banked_ram(S__currentTextBuffer+1,3,(uint8_t)(combined_text_buffer>>8));\n"
"        shake_banked_ram(S__displayTextAddressToCopyTo,3,(uint8_t)combined_text_destination); shake_banked_ram(S__displayTextAddressToCopyTo+1,3,(uint8_t)(combined_text_destination>>8)); shake_banked_ram(S__displayTextAddressToCopyToHigh,3,0); shake_banked_ram(S__displayTextAddressToCopyToHigh+1,3,0);\n"
"        for(unsigned i=0;i<=513;i++) { video_space_write(combined_text_destination+2*i,0x23); } combined_guard_trace_active=true;\n"
"        shake_banked_ram(S__b3PaletteRows,3,0);\n"
"        shake_ram(S__menuAllowed,1); shake_ram(S__menuShown,1); shake_ram(S__menuDirty,1);\n"
"        combined_sprite_records=combined_stage_latest_sprite_snapshot();\n"
"        shake_banked_ram(S_lastMainLoopCounter,6,(uint8_t)(shake_read(S__mainLoopCounter)-1));\n"
"        combined_injected=true; fprintf(stderr,\"Combined KQ1 workload staged: sound channels active, %u real menu(s), %u game-produced sprite record(s), 513 text bytes; invoking the compiled blocking display-text publisher\\n\",(unsigned)combined_menu_count,(unsigned)combined_sprite_records); combined_call_text_publisher(S__testRunCombinedDisplayText); return;\n"
"    }\n"
"    if(combined_irq_pending&&pc==S_custom_irq_handler) {\n"
"        uint8_t command=shake_banked_read(S_sendIrqCommand,6),work=shake_banked_read(S_irqWorkKind,6);\n"
"        combined_irq_entry_count++; combined_irq_entry_depth=shake_read(S__foregroundVeraDepth); combined_irq_entry_i=(regs.status&FLAG_INTERRUPT)!=0; combined_irq_entry_ram_bank=bank; combined_irq_entry_code_bank=regs.k; combined_irq_entry_command=command; combined_irq_entry_work=work; combined_irq_entry_generation=combined_read16(S_irqCompletionGeneration,6);\n"
"        if(combined_irq_entry_count<=4) fprintf(stderr,\"Combined IRQ entry %u: PC=%04x RAM_BANK=%02x PBR=%02x I=%u foreground_depth=%u foreground_active=%u command/work=%u/%u generation=%04x cycle=%u\\n\",combined_irq_entry_count,pc,combined_irq_entry_ram_bank,combined_irq_entry_code_bank,combined_irq_entry_i,combined_irq_entry_depth,foreground_active,command,work,combined_irq_entry_generation,clockticks6502);\n"
"        if(command==4||work==2) {\n"
"            combined_irq_pending=false; combined_irq_active=true;\n"
"            if(combined_irq_entry_depth) { fprintf(stderr,\"Blocking text publisher IRQ entered under foreground ownership: depth=%u command/work=%u/%u cycle=%u\\n\",combined_irq_entry_depth,command,work,clockticks6502); exit(3); }\n"
"            combined_row_calls=combined_sound_channels=combined_psg_writes=combined_audible_psg_writes=combined_menu_vram_writes=combined_sprite_vram_writes=combined_text_vram_writes=0; combined_row_command=command; combined_row_work_kind=work;\n"
"            combined_sound_active=combined_menu_active=combined_sprite_active=combined_text_active=false;\n"
"        } else combined_irq_pending=false;\n"
"    }\n"
"    if(!combined_irq_active) return;\n"
"    if(!(regs.status&FLAG_INTERRUPT)) combined_i_contiguous=false;\n"
"    if(combined_sound_active&&pc==combined_sound_return_pc&&(uint8_t)regs.sp==combined_sound_return_sp) combined_sound_active=false;\n"
"    if(combined_menu_active&&pc==combined_menu_return_pc&&(uint8_t)regs.sp==combined_menu_return_sp) combined_menu_active=false;\n"
"    if(combined_sprite_active&&pc==combined_sprite_return_pc&&(uint8_t)regs.sp==combined_sprite_return_sp) combined_sprite_active=false;\n"
"    if(combined_text_active&&pc==combined_text_return_pc&&(uint8_t)regs.sp==combined_text_return_sp) combined_text_active=false;\n"
"    if(bank==11&&pc==S_bBSoundHandler) { combined_row_calls|=1; combined_sound_channels=shake_banked_read(S__bBChannelsPlaying,11); combined_sound_active=true; combined_capture_call(&combined_sound_return_pc,&combined_sound_return_sp); }\n"
"    if(bank==10&&pc==S_bADisplayMenu) { combined_row_calls|=2; combined_menu_active=true; combined_capture_call(&combined_menu_return_pc,&combined_menu_return_sp); }\n"
"    if(bank==14&&pc==S_bEHandleSpriteUpdates) { combined_row_calls|=4; combined_sprite_active=true; combined_capture_call(&combined_sprite_return_pc,&combined_sprite_return_sp); }\n"
"    if(bank==3&&pc==S_handleDisplayTextSlice) { if(!combined_guard_seeded) { uint32_t guard=(uint32_t)combined_text_destination+2*513; uint8_t prior=video_space_read(guard); if(prior!=0x80||!combined_guard_menu_clear_seen) { fprintf(stderr,\"Combined guard did not observe the expected production menu clear before the first text slice: prior=%02x menu_clear_seen=%u menu_clear_writes=%u guard_writes=%u PC=%04x bank=%02x cycle=%u\\n\",prior,combined_guard_menu_clear_seen,combined_guard_menu_clear_writes,combined_guard_writes,pc,bank,clockticks6502); exit(3); } combined_guard_seed_active=true; video_space_write(guard,0x23); combined_guard_seed_active=false; if(video_space_read(guard)!=0x23) { fprintf(stderr,\"Could not seed the text overrun guard after the production menu clear: observed=%02x\\n\",video_space_read(guard)); exit(3); } combined_guard_seeded=true; fprintf(stderr,\"Seeded text overrun guard after production menu clear: prior=%02x writes=%u menu_clear_writes=%u PC=%04x bank=%02x cycle=%u\\n\",prior,combined_guard_writes,combined_guard_menu_clear_writes,pc,bank,clockticks6502); } else if(video_space_read((uint32_t)combined_text_destination+2*513)!=0x23||combined_guard_post_seed_writes) { fprintf(stderr,\"Text guard changed before the next bounded text slice: value=%02x writes_after_seed=%u PC=%04x bank=%02x cycle=%u\\n\",video_space_read((uint32_t)combined_text_destination+2*513),combined_guard_post_seed_writes,pc,bank,clockticks6502); exit(3); } combined_row_calls|=8; combined_text_active=true; combined_capture_call(&combined_text_return_pc,&combined_text_return_sp); }\n"
"}\n"
"static void combined_after_step(uint8_t opcode) {\n"
"    if(text_case_active) { uint16_t observed=combined_read16(S_irqCompletionGeneration,6); if(observed!=text_case_generation&&!text_case_generation_cycle) text_case_generation_cycle=clockticks6502; }\n"
"    bool completing_irq=combined_irq_active&&opcode==0x40&&regs.pc==combined_return_pc&&(uint8_t)regs.sp==combined_return_sp;\n"
"    if((combined_irq_pending||combined_irq_active)&&!completing_irq&&!(regs.status&FLAG_INTERRUPT)) combined_i_contiguous=false;\n"
"    if(combined_irq_active) { uint16_t observed=combined_read16(S_irqCompletionGeneration,6); if(observed!=(uint16_t)combined_generation&&!combined_generation_cycle) combined_generation_cycle=clockticks6502; }\n"
"    if(!completing_irq) return;\n"
"    combined_mask_end=clockticks6502;\n"
"    combined_finish_row();\n"
"    uint8_t command=shake_banked_read(S_sendIrqCommand,6),work=shake_banked_read(S_irqWorkKind,6); uint16_t generation=combined_read16(S_irqCompletionGeneration,6);\n"
"    if(combined_irq_entry_count<=4) fprintf(stderr,\"Combined IRQ RTI %u: row=%u PC=%04x SP=%04x entry_depth=%u current_depth=%u I=%u calls=%u text_writes=%u command/work=%u/%u generation=%04x cycle=%u\\n\",combined_irq_entry_count,combined_row_count,regs.pc,regs.sp,combined_irq_entry_depth,shake_read(S__foregroundVeraDepth),(regs.status&FLAG_INTERRUPT)!=0,combined_rows[combined_row_count-1].calls,combined_rows[combined_row_count-1].text_vram_writes,command,work,generation,clockticks6502);\n"
"    if(combined_row_count==1) { if(command||work!=2||generation!=(uint16_t)combined_generation||combined_rows[0].calls!=15||combined_rows[0].text_vram_writes!=512||combined_text_attribute_writes!=512||combined_text_attribute_pending||combined_rows[0].entry_depth) { fprintf(stderr,\"Combined first display-text slice did not defer after the full mixed IRQ row: command/work=%u/%u generation=%04x expected=%04x calls=%u glyphs=%u attributes=%u pending=%u depth=%u\\n\",command,work,generation,(uint16_t)combined_generation,combined_rows[0].calls,combined_rows[0].text_vram_writes,combined_text_attribute_writes,combined_text_attribute_pending,combined_rows[0].entry_depth); exit(3); } return; }\n"
"    if(command||work||generation!=(uint16_t)(combined_generation+1)||combined_rows[1].text_vram_writes!=1||combined_text_total_writes!=513||!combined_text_attributes_complete()) { fprintf(stderr,\"Combined text slices did not complete exactly once: command=%u work=%u generation=%04x expected=%04x row=%u cycle=%u row_calls=%u glyphs=%u attributes=%u total_glyphs=%u active=%u tail=%02x/%02x\\n\",command,work,generation,(uint16_t)(combined_generation+1),combined_row_count,clockticks6502,combined_rows[1].calls,combined_rows[1].text_vram_writes,combined_text_attribute_writes,combined_text_total_writes,combined_text_active,shake_banked_read(combined_text_buffer+512,3),shake_banked_read(combined_text_buffer+513,3)); exit(3); }\n"
"    uint32_t counter=combined_vsync_counter(); combined_completion_frames=counter-combined_inject_counter; combined_serviced_frames=combined_completion_frames; combined_asserted_frames=shake_frames_seen-combined_inject_frames; combined_complete=true; combined_write_csv(); fprintf(stderr,\"Combined IRQ work completed; waiting for the production blocking publisher to return before checking CPU and VRAM state\\n\");\n"
"}\n"
"static bool blit_current_entry_is_target(void) { uint16_t stride=shake_read(S__sizeOfViewTab),entry=(uint16_t)(S__viewtab+blit_target_entry_num*stride),caller=(uint16_t)(shake_read(S_VIEW_POS_LOCAL_VIEW_TAB)|((uint16_t)shake_read(S_VIEW_POS_LOCAL_VIEW_TAB+1)<<8)); return stride&&caller==entry; }\n"
"static void blit_snapshot_parent_viewtab(void) { blit_parent_viewtab_snapshot=0; blit_parent_viewtab_size=shake_read(S__sizeOfViewTab); if(!blit_parent_viewtab_size||blit_parent_viewtab_size>sizeof(blit_parent_viewtab_before)) { fprintf(stderr,\"Invalid caller ViewTable size %u\\n\",blit_parent_viewtab_size); exit(3); } uint16_t row=(uint16_t)(S__viewtab+blit_target_entry_num*blit_parent_viewtab_size); blit_loaded_view_num=shake_banked_read(row+shake_read(S__offsetOfCurrentView),9); blit_loaded_resource=blit_loaded_view_num==view_load_target&&shake_banked_read(S__loadedViews+blit_loaded_view_num*shake_read(S__sizeOfView),0x11)!=0; if(!blit_loaded_resource) { fprintf(stderr,\"Caller ViewTable entry %u does not hold retried view %u (found %u)\\n\",blit_target_entry_num,view_load_target,blit_loaded_view_num); exit(3); } for(unsigned i=0;i<blit_parent_viewtab_size;i++) blit_parent_viewtab_before[i]=shake_banked_read(row+i,9); blit_parent_viewtab_snapshot=1; }\n"
"static bool blit_parent_viewtab_unchanged(void) { if(!blit_parent_viewtab_snapshot) return false; uint16_t row=(uint16_t)(S__viewtab+blit_target_entry_num*blit_parent_viewtab_size); uint8_t prev_x=shake_read(S__offsetOfPrevX),prev_y=shake_read(S__offsetOfPrevY),stopped=shake_read(S__offsetOfStopped); for(unsigned i=0;i<blit_parent_viewtab_size;i++) if(i!=prev_x&&i!=prev_y&&i!=stopped&&shake_banked_read(row+i,9)!=blit_parent_viewtab_before[i]) return false; return true; }\n"
"static void blit_assert_partial_parent_return(void) { blit_parent_return_depth=shake_read(S__foregroundVeraDepth); unsigned final_new=0,event_allocated=0,find_events=0,baseline_reserved=blit_allocator_occupied_count(blit_saved_after_set_loop_allocator); bool reservations=blit_successful_loop_reservations_preserved(blit_saved_after_set_loop_allocator),parent_allocations=blit_parent_allocator_changes_valid(&final_new,&event_allocated,&find_events); if(blit_parent_return_depth||!blit_parent_viewtab_unchanged()||blit_result||!blit_bulk_helper_returned||blit_bulk_result||blit_bulk_find_entries!=2||!blit_partial_first_slot_seen||!blit_partial_map_rolled_back||!blit_partial_records_zero||!blit_c_allocator_returned||blit_c_allocator_result||!blit_allocator_restored||!reservations||!parent_allocations) { blit_report_parent_allocator_state(); fprintf(stderr,\"b9UpdateObjects allocation failure return failed: depth=%u result=%u helper=%u/%u finds=%u first/map/records=%u/%u/%u C=%u/%u restored=%u viewtab=%u loop_reservations=%u parent_allocations=%u events=%u event_blocks=%u final_new=%u baseline_reserved=%u\\n\",blit_parent_return_depth,blit_result,blit_bulk_helper_returned,blit_bulk_result,blit_bulk_find_entries,blit_partial_first_slot_seen,blit_partial_map_rolled_back,blit_partial_records_zero,blit_c_allocator_returned,blit_c_allocator_result,blit_allocator_restored,blit_parent_viewtab_unchanged(),reservations,parent_allocations,find_events,event_allocated,final_new,baseline_reserved); exit(3); } fprintf(stderr,\"b9UpdateObjects partial allocation rollback: exact constrained-map rollback and zeroed output records propagated false through C/agiBlit; after restore, %u occupied blocks were retained from successful bESetLoop and %u new blocks matched %u real find-block events for other ViewTable owners\\n\",baseline_reserved,event_allocated,find_events); }\n"
"static uint16_t blit_read16_banked(uint16_t address,uint8_t bank) { return (uint16_t)(shake_banked_read(address,bank)|((uint16_t)shake_banked_read(address+1,bank)<<8)); }\n"
"static uint32_t blit_read32_banked(uint16_t address,uint8_t bank) { return (uint32_t)shake_banked_read(address,bank)|((uint32_t)shake_banked_read(address+1,bank)<<8)|((uint32_t)shake_banked_read(address+2,bank)<<16)|((uint32_t)shake_banked_read(address+3,bank)<<24); }\n"
"static bool blit_vram_range_reserved(uint32_t address,uint8_t blocks) { if(address<0xea00u||address>=0x1f800u||((address-0xea00u)&31u)) return false; unsigned first=(address-0xea00u)/32u; if(first+blocks>2160u) return false; for(unsigned i=0;i<blocks;i++) if(shake_banked_read((uint16_t)(S__bDSpriteAllocTable+first+i),13)!=1) return false; return true; }\n"
"static bool blit_snapshot_loop_ranges_reserved(const BlitMetadataSnapshot *snapshot) { if(!snapshot->count||!snapshot->blocks) return false; for(unsigned i=0;i<snapshot->count;i++) if(!blit_vram_range_reserved(snapshot->loop[i],snapshot->blocks)) return false; return true; }\n"
"static BlitMetadataSnapshot blit_capture_metadata(uint8_t entry) { BlitMetadataSnapshot out={0}; uint8_t offset_back=shake_read(S__offsetOfBackBuffers),offset_loops=shake_read(S__offsetOfloopsVeraAddressesPointers),offset_bank=shake_read(S__offsetOfViewMetadataBank),offset_on=shake_read(S__offsetOfIsOnBackBuffer); uint16_t meta=(uint16_t)(S__viewTableMetadata+entry*(offset_back+4)); out.metadata_bank=shake_banked_read(meta+offset_bank,14); out.is_on_backbuffer=shake_banked_read(meta+offset_on,14); out.backbuffer_size=shake_banked_read(meta+offset_on+1,14); out.count=0; uint16_t array=blit_read16_banked(meta+offset_loops,14); out.backbuffer_records=blit_read16_banked(meta+offset_back,14); uint16_t tab=(uint16_t)(S__viewtab+entry*shake_read(S__sizeOfViewTab)); uint8_t view=shake_banked_read(tab+shake_read(S__offsetOfCurrentView),9),loop=shake_banked_read(tab+shake_read(S__offsetOfCurrentLoop),9),cel=shake_banked_read(tab+shake_read(S__offsetOfCurrentCel),9); out.view_num=view; out.loop_num=loop; out.cel_num=cel; uint16_t loaded=(uint16_t)(S__loadedViews+view*shake_read(S__sizeOfView)); uint8_t max_slots=shake_banked_read(loaded+shake_read(S__offsetOfMaxVeraSlots),0x11),loops_bank=shake_banked_read(loaded+shake_read(S__offsetOfLoopsBank),0x11); uint16_t loops=blit_read16_banked(loaded+shake_read(S__offsetOfLoops),0x11); if(max_slots>0) { out.count=max_slots; uint16_t loops_array=blit_read16_banked(array+loop*2,out.metadata_bank); out.loop_records=(uint16_t)(loops_array+cel*max_slots*4u); uint16_t loop_data=(uint16_t)(loops+loop*shake_read(S__sizeOfLoop)); out.width=shake_banked_read(loop_data+shake_read(S__offsetOfAllocationWidth),loops_bank); out.height=shake_banked_read(loop_data+shake_read(S__offsetOfAllocationHeight),loops_bank); if(out.width<4&&out.height<4) out.blocks=(uint8_t)((1u<<out.width)*(1u<<out.height)); } out.loop_zero=out.backbuffer_zero=1; out.loop_valid=out.backbuffer_valid=(out.count>0&&out.loop_records&&out.backbuffer_records); out.loop_reserved=out.backbuffer_reserved=1; for(unsigned i=0;i<out.count;i++) { out.loop[i]=out.loop_records?blit_read32_banked(out.loop_records+i*4u,out.metadata_bank):0; out.backbuffer[i]=out.backbuffer_records?blit_read32_banked(out.backbuffer_records+i*4u,out.metadata_bank):0; if(out.loop[i]) out.loop_zero=0; if(out.backbuffer[i]) out.backbuffer_zero=0; if(!out.loop[i]||out.loop[i]>=0x1f800u||((out.loop[i]-0xea00u)&31u)) out.loop_valid=0; if(!out.backbuffer[i]||out.backbuffer[i]>=0x1f800u||((out.backbuffer[i]-0xea00u)&31u)) out.backbuffer_valid=0; if(!out.blocks||!blit_vram_range_reserved(out.loop[i],out.blocks)) out.loop_reserved=0; if(!out.blocks||!blit_vram_range_reserved(out.backbuffer[i],out.blocks)) out.backbuffer_reserved=0; } return out; }\n"
"static bool blit_same_records(const uint32_t *a,const uint32_t *b,uint8_t count) { for(unsigned i=0;i<count;i++) if(a[i]!=b[i]) return false; return true; }\n"
"static bool blit_same_backbuffer_metadata(const BlitMetadataSnapshot *a,const BlitMetadataSnapshot *b) { return a->count==b->count&&a->backbuffer_records==b->backbuffer_records&&a->metadata_bank==b->metadata_bank&&a->is_on_backbuffer==b->is_on_backbuffer&&a->backbuffer_size==b->backbuffer_size&&blit_same_records(a->backbuffer,b->backbuffer,a->count); }\n"
"static void blit_save_allocator_state(uint8_t *table,uint8_t *code) { for(int i=0;i<2160;i++) table[i]=shake_banked_read(S__bDSpriteAllocTable+i,13); code[0]=shake_banked_read(S_findFreeVRamLowByteLoop+1,13); code[1]=shake_banked_read(S_findFreeVRamLowByteLoop+2,13); code[2]=shake_banked_read(S_findFirstFreeVRamBlock_optimisticSkip,13); code[3]=shake_banked_read(S_findFirstFreeVRamBlock_optimisticSkip+1,13); }\n"
"static void blit_save_allocator(void) { blit_save_allocator_state(blit_saved_allocator,blit_saved_allocator_code); }\n"
"static bool blit_allocator_map_equals(const uint8_t *table) { for(int i=0;i<2160;i++) if(shake_banked_read(S__bDSpriteAllocTable+i,13)!=table[i]) return false; return shake_banked_read(S__bDSpriteAllocTable+2160,13)==0xff; }\n"
"static bool blit_original_reservations_preserved(const uint8_t *table) { for(int i=0;i<2160;i++) if(blit_saved_allocator[i]==1&&table[i]!=1) return false; return true; }\n"
"static bool blit_successful_loop_reservations_preserved(const uint8_t *table) { for(unsigned i=0;i<2160;i++) { uint8_t current=shake_banked_read(S__bDSpriteAllocTable+i,13); if(table[i]>1||current>1||(table[i]==1&&current!=1)) return false; } return shake_banked_read(S__bDSpriteAllocTable+2160,13)==0xff; }\n"
"static bool blit_parent_allocator_changes_valid(unsigned *final_new_blocks,unsigned *event_allocated_blocks,unsigned *find_events) { unsigned final_new=0,event_allocated=0,find_count=0; if(blit_allocator_trace_depth||blit_allocator_trace_overflow||blit_allocator_event_overflow||blit_allocator_trace_calls!=blit_allocator_event_count) return false; for(unsigned e=0;e<blit_allocator_event_count;e++) { const BlitAllocatorEvent *event=&blit_allocator_events[e]; if(event->kind!=2||event->entry==blit_target_entry_num||event->entry>=20||event->owner_tab<0xa000||event->owner_tab>=0xc000||event->view==0xff||event->loop==0xff||event->cel==0xff||!event->allocated||event->changed!=event->allocated||event->freed||event->other||event->first_allocated>=2160||event->first_allocated+event->allocated>2160||event->requested_blocks!=event->allocated||event->returned_address!=(0xea00u+(uint32_t)event->first_allocated*32u)) return false; find_count++; event_allocated+=event->allocated; for(unsigned j=0;j<event->allocated;j++) { unsigned block=event->first_allocated+j; if(blit_saved_after_set_loop_allocator[block]!=0||shake_banked_read(S__bDSpriteAllocTable+block,13)!=1) return false; } } for(unsigned i=0;i<2160;i++) { uint8_t before=blit_saved_after_set_loop_allocator[i],after=shake_banked_read(S__bDSpriteAllocTable+i,13); if(before>1||after>1||(before==1&&after!=1)) return false; if(before==0&&after==1) { unsigned covers=0; final_new++; for(unsigned e=0;e<blit_allocator_event_count;e++) { const BlitAllocatorEvent *event=&blit_allocator_events[e]; if(i>=event->first_allocated&&i<event->first_allocated+event->allocated) covers++; } if(covers!=1) return false; } } if(final_new_blocks) *final_new_blocks=final_new; if(event_allocated_blocks) *event_allocated_blocks=event_allocated; if(find_events) *find_events=find_count; return blit_allocator_event_count==2&&find_count==2&&final_new==event_allocated; }\n"
"static unsigned blit_allocator_occupied_count(const uint8_t *table) { unsigned count=0; for(unsigned i=0;i<2160;i++) if(table[i]==1) count++; return count; }\n"
"static unsigned blit_first_map_mismatch(const uint8_t *expected,uint8_t *expected_value,uint8_t *actual_value) { for(unsigned i=0;i<2160;i++) { uint8_t actual=shake_banked_read(S__bDSpriteAllocTable+i,13); if(actual!=expected[i]) { *expected_value=expected[i]; *actual_value=actual; return i; } } *expected_value=0xff; *actual_value=shake_banked_read(S__bDSpriteAllocTable+2160,13); return *actual_value==0xff?2161:2160; }\n"
"static void blit_report_parent_allocator_state(void) { bool map=blit_allocator_map_equals(blit_saved_after_set_loop_allocator),state=blit_allocator_skip_code_valid(blit_saved_after_set_loop_code),reservations=blit_successful_loop_reservations_preserved(blit_saved_after_set_loop_allocator); unsigned final_new=0,event_allocated=0,find_events=0; bool accounted=blit_parent_allocator_changes_valid(&final_new,&event_allocated,&find_events); uint8_t expected=0xff,actual=0xff,code[4]; unsigned first=blit_first_map_mismatch(blit_saved_after_set_loop_allocator,&expected,&actual),first_set_loop=2161; uint8_t set_loop_before=0xff,set_loop_after=0xff; for(unsigned i=0;i<2160;i++) if(blit_saved_allocator[i]!=blit_saved_after_set_loop_allocator[i]) { first_set_loop=i; set_loop_before=blit_saved_allocator[i]; set_loop_after=blit_saved_after_set_loop_allocator[i]; break; } code[0]=shake_banked_read(S_findFreeVRamLowByteLoop+1,13); code[1]=shake_banked_read(S_findFreeVRamLowByteLoop+2,13); code[2]=shake_banked_read(S_findFirstFreeVRamBlock_optimisticSkip,13); code[3]=shake_banked_read(S_findFirstFreeVRamBlock_optimisticSkip+1,13); fprintf(stderr,\"b9 allocator parent-return diagnostics: exact-map/skip-code-valid=%u/%u successful-loop-reservations=%u accounted-parent-events=%u events/blocks/final-new=%u/%u/%u baseline-reserved=%u first-parent-mismatch=%u:%02x->%02x first-bESetLoop-delta=%u:%02x->%02x trace-calls/printed/open/overflow=%u/%u/%u/%u\\n\",map,state,reservations,accounted,find_events,event_allocated,final_new,blit_allocator_occupied_count(blit_saved_after_set_loop_allocator),first,expected,actual,first_set_loop,set_loop_before,set_loop_after,blit_allocator_trace_calls,blit_allocator_trace_printed,blit_allocator_trace_depth,blit_allocator_trace_overflow); fprintf(stderr,\"b9 allocator scan code bytes expected=%02x/%02x/%02x/%02x actual=%02x/%02x/%02x/%02x table-terminator=%02x\\n\",blit_saved_after_set_loop_code[0],blit_saved_after_set_loop_code[1],blit_saved_after_set_loop_code[2],blit_saved_after_set_loop_code[3],code[0],code[1],code[2],code[3],shake_banked_read(S__bDSpriteAllocTable+2160,13)); }\n"
"static uint8_t blit_allocator_owner(uint8_t *entry,uint16_t *owner_tab,uint8_t *view,uint8_t *loop,uint8_t *cel) { *entry=shake_read(S_VIEW_POS_ENTRY_NUM); *owner_tab=(uint16_t)(shake_read(S_VIEW_POS_LOCAL_VIEW_TAB)|((uint16_t)shake_read(S_VIEW_POS_LOCAL_VIEW_TAB+1)<<8)); *view=*loop=*cel=0xff; if(*owner_tab>=0xa000&&*owner_tab<0xc000) { *view=shake_banked_read(*owner_tab+shake_read(S__offsetOfCurrentView),9); *loop=shake_banked_read(*owner_tab+shake_read(S__offsetOfCurrentLoop),9); *cel=shake_banked_read(*owner_tab+shake_read(S__offsetOfCurrentCel),9); } return *entry; }\n"
"static void blit_allocator_trace_step(uint16_t pc,uint8_t bank) {\n"
"    if(blit_allocator_trace_depth) { BlitAllocatorTraceFrame *frame=&blit_allocator_trace_stack[blit_allocator_trace_depth-1]; if(pc==frame->return_pc&&(uint8_t)regs.sp==frame->return_sp) { unsigned changed=0,allocated=0,freed=0,other=0; uint16_t first=0xffff; uint8_t first_before=0xff,first_after=0xff; unsigned depth=blit_allocator_trace_depth-1; for(unsigned i=0;i<2160;i++) { uint8_t old=blit_allocator_trace_maps[depth][i],value=shake_banked_read(S__bDSpriteAllocTable+i,13); if(value!=old) { if(changed==0) { first_before=old; first_after=value; } changed++; if(old==0&&value==1) { if(first==0xffff) first=(uint16_t)i; else if(i!=(unsigned)first+allocated) other++; allocated++; } else if(old==1&&value==0) freed++; else other++; } } BlitAllocatorEvent *event=NULL; if(blit_allocator_event_count<BLIT_ALLOCATOR_TRACE_EVENTS) { event=&blit_allocator_events[blit_allocator_event_count++]; event->kind=frame->kind; event->entry=frame->entry; event->owner_tab=frame->owner_tab; event->view=frame->view; event->loop=frame->loop; event->cel=frame->cel; event->changed=(uint16_t)changed; event->allocated=(uint16_t)allocated; event->freed=(uint16_t)freed; event->other=(uint16_t)other; event->first_allocated=first; event->requested_blocks=shake_read(S_BLOCKS_TO_FIND); event->returned_address=(uint32_t)regs.a|((uint32_t)regs.xl<<8)|((uint32_t)shake_read(S_sreg)<<16)|((uint32_t)shake_read(S_sreg+1)<<24); event->return_carry=(regs.status&FLAG_CARRY)!=0; } else blit_allocator_event_overflow++; if(blit_allocator_trace_printed<BLIT_ALLOCATOR_TRACE_PRINTS) { static const char *kind_names[]={\"allocate-one\",\"allocate-bulk\",\"find-block\",\"delete-c\",\"delete-asm\"}; fprintf(stderr,\"b9 allocator event #%u %s owner=%u/%04x view-loop-cel=%u/%u/%u pc=%02x:%04x->%02x:%04x cycle=%u..%u args=%02x/%02x/%02x request=%u return=%06x carry=%u map_changes=%u allocations=%u frees=%u other=%u first=%u:%02x->%02x\\n\",blit_allocator_trace_calls,kind_names[frame->kind],frame->entry,frame->owner_tab,frame->view,frame->loop,frame->cel,frame->bank,frame->entry_pc,bank,pc,frame->cycle,clockticks6502,frame->a,frame->x,frame->y,shake_read(S_BLOCKS_TO_FIND),(unsigned)((uint32_t)regs.a|((uint32_t)regs.xl<<8)|((uint32_t)shake_read(S_sreg)<<16)|((uint32_t)shake_read(S_sreg+1)<<24)),(regs.status&FLAG_CARRY)!=0,changed,allocated,freed,other,first,first_before,first_after); blit_allocator_trace_printed++; } blit_allocator_trace_depth--; } }\n"
"    uint8_t kind=0xff; if(bank==0x0e&&pc==S__bEAllocateSpriteMemory) kind=0; else if(bank==0x0e&&pc==S__bEAllocateSpriteMemoryBulk) kind=1; else if(bank==0x0d&&pc==S_bDFindFreeVramBlock) kind=2; else if(bank==0x0d&&pc==S__bDDeleteAllocation) kind=3; else if(bank==0x0d&&pc==S_bDDeleteAllocationAsmCall) kind=4; if(kind==0xff) return;\n"
"    if(blit_allocator_trace_depth>=BLIT_ALLOCATOR_TRACE_DEPTH) { blit_allocator_trace_overflow++; return; } BlitAllocatorTraceFrame *frame=&blit_allocator_trace_stack[blit_allocator_trace_depth]; frame->kind=kind; frame->bank=bank; frame->entry_pc=pc; frame->a=regs.a; frame->x=regs.xl; frame->y=regs.yl; frame->cycle=clockticks6502; frame->return_pc=shake_call_return(&frame->return_sp); blit_allocator_owner(&frame->entry,&frame->owner_tab,&frame->view,&frame->loop,&frame->cel); for(unsigned i=0;i<2160;i++) blit_allocator_trace_maps[blit_allocator_trace_depth][i]=shake_banked_read(S__bDSpriteAllocTable+i,13); blit_allocator_trace_depth++; blit_allocator_trace_calls++;\n"
"}\n"
"static bool blit_map_all_occupied(void) { for(int i=0;i<2160;i++) if(shake_banked_read(S__bDSpriteAllocTable+i,13)!=1) return false; return shake_banked_read(S__bDSpriteAllocTable+2160,13)==0xff; }\n"
"static bool blit_bulk_records_zero(void) { for(int i=0;i<8;i++) if(shake_banked_read(S__bEBulkAllocatedAddresses+i,0x0e)) return false; return true; }\n"
"static bool blit_allocator_skip_code_valid(const uint8_t *before_code) {"
"    uint8_t opcode=shake_banked_read(S_findFirstFreeVRamBlock_optimisticSkip,13),operand=shake_banked_read(S_findFirstFreeVRamBlock_optimisticSkip+1,13);"
"    return (opcode==0xea&&operand==0xea)||(opcode==before_code[2]&&operand==before_code[3]&&opcode==0x80);"
"}"
"static bool blit_allocator_continuation_valid(const uint8_t *before_code) {"
"    uint16_t pointer=(uint16_t)(shake_banked_read(S_findFreeVRamLowByteLoop+1,13)|((uint16_t)shake_banked_read(S_findFreeVRamLowByteLoop+2,13)<<8));"
"    return pointer==S__bDSpriteAllocTable&&shake_banked_read(S__bDSpriteAllocTable+2160,13)==0xff&&blit_allocator_skip_code_valid(before_code);"
"}"
"static bool blit_partial_fixture_map_valid(void) {"
"    if(blit_fixture_gap_address<0xea00u||blit_fixture_gap_address>=0x1f800u||((blit_fixture_gap_address-0xea00u)&31u)||!blit_fixture_gap_blocks) return false;"
"    unsigned first=(blit_fixture_gap_address-0xea00u)/32u;"
"    if(first+blit_fixture_gap_blocks>2160u) return false;"
"    for(unsigned i=0;i<2160;i++) {"
"        uint8_t before=blit_saved_after_set_loop_allocator[i],actual=shake_banked_read(S__bDSpriteAllocTable+i,13);"
"        if(before>1) return false;"
"        uint8_t expected=(i>=first&&i<first+blit_fixture_gap_blocks)?0:1;"
"        if((before==1&&expected==0)||actual!=expected) return false;"
"    }"
"    return shake_banked_read(S__bDSpriteAllocTable+2160,13)==0xff;"
"}"
"static void blit_fixture_begin(uint8_t op,uint16_t target) {"
"    if(blit_fixture_call_active||!regs.e||(regs.sp&0xff00)!=0x0100) { fprintf(stderr,\"Sprite pressure helper entry contract failed: active=%u E=%u SP=%04x\\n\",blit_fixture_call_active,regs.e,regs.sp); exit(3); }"
"    if(op==1||op==2) {"
"        blit_fixture_kind=op; blit_fixture_setup_entries=blit_fixture_find_entries=blit_fixture_delete_entries=blit_fixture_helper_entries=blit_fixture_release_entries=0;"
"        blit_fixture_owned_blocks=blit_fixture_released_blocks=blit_fixture_gap_blocks=0; blit_fixture_gap_address=0;"
"        blit_fixture_cleanup_started=blit_fixture_cleanup_done=false;"
"        if(op==2) {"
"            if(blit_before_set_loop.width>=4||blit_before_set_loop.height>=4||blit_before_set_loop.count!=2||!blit_before_set_loop.blocks) { fprintf(stderr,\"VIEW 33 real partial fixture has invalid target geometry: slots=%u width=%u height=%u blocks=%u\\n\",blit_before_set_loop.count,blit_before_set_loop.width,blit_before_set_loop.height,blit_before_set_loop.blocks); exit(3); }"
"            shake_ram(S__testSpriteFixtureWidth,(uint8_t)(8u<<blit_before_set_loop.width));"
"            shake_ram(S__testSpriteFixtureHeight,(uint8_t)(8u<<blit_before_set_loop.height));"
"        }"
"    }"
"    blit_fixture_op=op; blit_fixture_target=target; blit_fixture_helper_entries=0;"
"    blit_fixture_return_pc=regs.pc; blit_fixture_return_sp=regs.sp; blit_fixture_c_sp=combined_read16(S_c_sp,0);"
"    blit_fixture_saved_a=regs.a; blit_fixture_saved_x=regs.xl; blit_fixture_saved_y=regs.yl; blit_fixture_saved_status=regs.status;"
"    blit_fixture_saved_db=regs.db; blit_fixture_saved_code_bank=regs.k; blit_fixture_saved_e=regs.e;"
"    blit_fixture_saved_ram_bank=memory_get_ram_bank(); blit_fixture_saved_rom_bank=memory_get_rom_bank(); blit_fixture_saved_depth=shake_read(S__foregroundVeraDepth);"
"    if(op==3&&blit_fixture_saved_a!=0) { fprintf(stderr,\"agiBlit returned success at allocator-failure cleanup boundary: A=%u\\n\",blit_fixture_saved_a); exit(3); }"
"    blit_fixture_call_active=true;"
"    shake_push_test_jsr(target);"
"}"
"static void blit_fixture_finish_call(void) {"
"    uint16_t c_sp=combined_read16(S_c_sp,0);"
"    uint8_t depth=shake_read(S__foregroundVeraDepth);"
"    bool caller_state_ok=regs.sp==blit_fixture_return_sp&&regs.e==blit_fixture_saved_e&&regs.k==blit_fixture_saved_code_bank&&regs.db==blit_fixture_saved_db&&memory_get_ram_bank()==blit_fixture_saved_ram_bank&&memory_get_rom_bank()==blit_fixture_saved_rom_bank&&c_sp==blit_fixture_c_sp&&depth==blit_fixture_saved_depth&&((regs.status^blit_fixture_saved_status)&FLAG_INTERRUPT)==0&&blit_current_entry_is_target();"
"    if(!caller_state_ok||blit_fixture_helper_entries!=1) { fprintf(stderr,\"Sprite pressure helper return failed: op=%u helper=%u SP=%04x/%04x Cstack=%04x/%04x banks=%u/%u=>%u/%u code/db=%u/%u=>%u/%u depth=%u/%u I=%u/%u target=%u\\n\",blit_fixture_op,blit_fixture_helper_entries,regs.sp,blit_fixture_return_sp,c_sp,blit_fixture_c_sp,blit_fixture_saved_ram_bank,blit_fixture_saved_rom_bank,memory_get_ram_bank(),memory_get_rom_bank(),blit_fixture_saved_code_bank,blit_fixture_saved_db,regs.k,regs.db,depth,blit_fixture_saved_depth,(regs.status&FLAG_INTERRUPT)!=0,(blit_fixture_saved_status&FLAG_INTERRUPT)!=0,blit_current_entry_is_target()); exit(3); }"
"    uint8_t error=shake_banked_read(S__testSpriteFixtureError,0);"
"    uint16_t allocations=blit_read16_banked(S__testSpriteFixtureAllocationCount,0);"
"    if(blit_fixture_op==1||blit_fixture_op==2) {"
"        if(error) { fprintf(stderr,\"Sprite pressure setup helper reported error %u (kind=%u)\\n\",error,blit_fixture_kind); exit(3); }"
"        blit_fixture_owned_blocks=allocations;"
"        if(!allocations) { fprintf(stderr,\"Sprite pressure setup helper observed no owned blocks (kind=%u)\\n\",blit_fixture_kind); exit(3); }"
"        if(blit_fixture_kind==1) {"
"            unsigned free_before=2160u-blit_allocator_occupied_count(blit_saved_allocator);"
"            if(!blit_map_all_occupied()||allocations!=free_before) { fprintf(stderr,\"Full-pressure setup did not exhaust every free allocator block: owned=%u free-before=%u all-occupied=%u\\n\",allocations,free_before,blit_map_all_occupied()); exit(3); }"
"            for(unsigned i=0;i<2160;i++) blit_fixture_allocator_before[i]=shake_banked_read(S__bDSpriteAllocTable+i,13);"
"            if(!blit_original_reservations_preserved(blit_fixture_allocator_before)) { fprintf(stderr,\"Full-pressure setup cleared an original sprite owner\\n\"); exit(3); }"
"        } else {"
"            blit_fixture_gap_blocks=shake_banked_read(S__testSpriteFixtureGapBlocks,0);"
"            blit_fixture_gap_address=blit_read32_banked(S__testSpriteFixtureGapAddress,0);"
"            blit_partial_expected_address=blit_fixture_gap_address;"
"            unsigned expected_owned=2160u-blit_allocator_occupied_count(blit_saved_after_set_loop_allocator)-blit_fixture_gap_blocks;"
"            if(blit_fixture_gap_blocks!=blit_before_set_loop.blocks||allocations!=expected_owned||!blit_partial_fixture_map_valid()) { fprintf(stderr,\"Partial-pressure setup did not leave exactly one free request run: owned=%u expected=%u gap=%06lx/%u wanted=%u valid=%u\\n\",allocations,expected_owned,(unsigned long)blit_fixture_gap_address,blit_fixture_gap_blocks,blit_before_set_loop.blocks,blit_partial_fixture_map_valid()); exit(3); }"
"            for(unsigned i=0;i<2160;i++) blit_fixture_allocator_before[i]=shake_banked_read(S__bDSpriteAllocTable+i,13);"
"        }"
"    } else if(blit_fixture_op==3) {"
"        uint16_t released=blit_read16_banked(S__testSpriteFixtureReleasedCount,0);"
"        if(error||blit_fixture_release_entries!=1||released!=blit_fixture_owned_blocks||allocations||!blit_allocator_map_equals(blit_fixture_kind==1?blit_saved_allocator:blit_saved_after_set_loop_allocator)||!blit_allocator_continuation_valid(blit_fixture_kind==1?blit_saved_allocator_code:blit_saved_after_set_loop_code)) { fprintf(stderr,\"Sprite pressure cleanup failed: kind=%u error=%u release=%u owned=%u released=%u remaining=%u map/cursor=%u/%u\\n\",blit_fixture_kind,error,blit_fixture_release_entries,blit_fixture_owned_blocks,released,allocations,blit_allocator_map_equals(blit_fixture_kind==1?blit_saved_allocator:blit_saved_after_set_loop_allocator),blit_allocator_continuation_valid(blit_fixture_kind==1?blit_saved_allocator_code:blit_saved_after_set_loop_code)); exit(3); }"
"        blit_fixture_released_blocks=released; blit_fixture_cleanup_done=true; blit_allocator_restored=blit_fixture_kind==2;"
"        if(blit_fixture_kind==1) fprintf(stderr,\"VIEW fixture full: setup=%u find=%u delete=%u owned=%u released=%u\\n\",blit_fixture_setup_entries,blit_fixture_find_entries,blit_fixture_delete_entries,blit_fixture_owned_blocks,blit_fixture_released_blocks);"
"        else fprintf(stderr,\"VIEW fixture partial: setup=%u find=%u delete=%u owned=%u released=%u gap_blocks=%u\\n\",blit_fixture_setup_entries,blit_fixture_find_entries,blit_fixture_delete_entries,blit_fixture_owned_blocks,blit_fixture_released_blocks,blit_fixture_gap_blocks);"
"    } else { fprintf(stderr,\"Unexpected sprite pressure helper op %u\\n\",blit_fixture_op); exit(3); }"
"    blit_fixture_call_active=false;"
"    regs.a=blit_fixture_saved_a; regs.xl=blit_fixture_saved_x; regs.yl=blit_fixture_saved_y; regs.status=blit_fixture_saved_status;"
"}"
"static void blit_save_viewtab_flags(void) { uint8_t flags_offset=shake_read(S__offsetOfFlags); uint16_t flags=(uint16_t)(S__viewtab+blit_target_entry_num*shake_read(S__sizeOfViewTab)+flags_offset); blit_view_flags[0]=shake_banked_read(flags,9); blit_view_flags[1]=shake_banked_read(flags+1,9); blit_saved_view_flags=true; }\n"
"static void blit_restore_viewtab_flags(void) { uint8_t flags_offset=shake_read(S__offsetOfFlags); uint16_t row=(uint16_t)(S__viewtab+blit_target_entry_num*shake_read(S__sizeOfViewTab)); uint16_t flags=(uint16_t)(row+flags_offset); if(blit_saved_view_flags) { shake_banked_ram(flags,9,blit_view_flags[0]); shake_banked_ram(flags+1,9,blit_view_flags[1]); blit_saved_view_flags=false; } if(blit_saved_stale_counter) { uint16_t stale=(uint16_t)(row+shake_read(S__offsetOfStopped)-1); shake_banked_ram(stale,9,blit_stale_counter); blit_saved_stale_counter=false; } }\n"
"static void picture_inject_blit_commands(void) { const uint8_t code[]={30,view_load_target,41,1,view_load_target,35,1}; blit_target_entry_num=1; blit_save_viewtab_flags(); blit_load_view_entries=blit_set_view_entries=blit_draw_entries=0; picture_save_injection(code,sizeof(code)); picture_test_stage=5; }\n"
"static uint16_t shake_call_return(uint8_t *return_sp) { uint8_t sp=(uint8_t)regs.sp; *return_sp=(uint8_t)(sp+2); return (uint16_t)(shake_read(0x0100+(uint8_t)(sp+1))|((uint16_t)shake_read(0x0100+(uint8_t)(sp+2))<<8))+1; }\n"
"static void shake_push_test_jsr(uint16_t target) { if(!regs.e||(regs.sp&0xff00)!=0x0100) { fprintf(stderr,\"Synthetic test JSR requires the emulation-mode page-one stack: E=%u SP=%04x\\n\",regs.e,regs.sp); exit(3); } uint16_t return_address=(uint16_t)(regs.pc-1); shake_ram(regs.sp,(uint8_t)(return_address>>8)); regs.sp=(regs.sp&0xff00)|((regs.sp-1)&0xff); shake_ram(regs.sp,(uint8_t)return_address); regs.sp=(regs.sp&0xff00)|((regs.sp-1)&0xff); regs.pc=target; }\n"
"static void shake_log_sprite_owner_schedule(uint16_t target,uint8_t is_end,uint8_t expected_depth) { fprintf(stderr,\"Scheduling real sprite-owner wrapper: end=%u PC=%04x SP=%02x banks=%u/%u codebank=%u depth=%u expected=%u Cstack=%04x target=%04x I=%u\\n\",is_end,regs.pc,(uint8_t)regs.sp,memory_get_ram_bank(),memory_get_rom_bank(),regs.k,shake_read(S__foregroundVeraDepth),expected_depth,combined_read16(S_c_sp,0),target,(regs.status&FLAG_INTERRUPT)!=0); }\n"
"static void shake_check_outer_owner_interval(uint8_t ending) { if(!ending) { shake_view_warm_outer_frames_start=shake_frames_seen; shake_view_warm_outer_service_start=foreground_vsync_counter(); shake_view_warm_outer_handlers_start=shake_view_warm_sprite_handler_entries; return; } uint32_t asserted=shake_frames_seen-shake_view_warm_outer_frames_start,serviced=foreground_vsync_counter()-shake_view_warm_outer_service_start; unsigned sprite_handlers=shake_view_warm_sprite_handler_entries-shake_view_warm_outer_handlers_start; if(!asserted||!serviced||sprite_handlers) { fprintf(stderr,\"Outer foreground owner IRQ coverage failed: asserted=%u serviced=%u sprite_handlers=%u\\n\",asserted,serviced,sprite_handlers); exit(3); } fprintf(stderr,\"Outer foreground owner serviced VSYNC while deferring sprite publication: asserted=%u serviced=%u sprite_handlers=%u\\n\",asserted,serviced,sprite_handlers); }\n"
"static void shake_call_sprite_owner_wrapper(uint16_t target,uint8_t is_end,uint8_t expected_depth,uint8_t result_depth) { shake_log_sprite_owner_schedule(target,is_end,expected_depth); if(shake_sprite_owner_wrapper_active) { fprintf(stderr,\"Nested sprite-owner fixture wrapper call\\n\"); exit(3); } uint8_t depth=shake_read(S__foregroundVeraDepth); if(depth!=expected_depth || (regs.status&FLAG_INTERRUPT) || !regs.e || (regs.sp&0xff00)!=0x0100) { fprintf(stderr,\"Sprite-owner wrapper entry contract failed: end=%u depth=%u expected=%u I=%u E=%u SP=%04x\\n\",is_end,depth,expected_depth,(regs.status&FLAG_INTERRUPT)!=0,regs.e,regs.sp); exit(3); } shake_sprite_owner_wrapper_return_pc=regs.pc; shake_sprite_owner_wrapper_return_sp=regs.sp; shake_sprite_owner_wrapper_c_sp=combined_read16(S_c_sp,0); shake_sprite_owner_wrapper_ram_bank=memory_get_ram_bank(); shake_sprite_owner_wrapper_rom_bank=memory_get_rom_bank(); shake_sprite_owner_wrapper_code_bank=regs.k; shake_sprite_owner_wrapper_status=regs.status; shake_sprite_owner_wrapper_a=regs.a; shake_sprite_owner_wrapper_x=regs.xl; shake_sprite_owner_wrapper_y=regs.yl; shake_sprite_owner_wrapper_expected_depth=expected_depth; shake_sprite_owner_wrapper_result_depth=result_depth; shake_sprite_owner_wrapper_is_end=is_end; shake_sprite_owner_wrapper_progress_logged=false; shake_sprite_owner_wrapper_start_cycle=clockticks6502; shake_push_test_jsr(target); shake_sprite_owner_wrapper_active=true; }\n"
"static void shake_check_sprite_owner_wrapper_return(void) { if(!shake_sprite_owner_wrapper_active || regs.pc!=shake_sprite_owner_wrapper_return_pc) return; if(regs.sp!=shake_sprite_owner_wrapper_return_sp || !regs.e || (regs.sp&0xff00)!=0x0100) { fprintf(stderr,\"Sprite-owner wrapper changed hardware stack: end=%u SP=%04x expected=%04x E=%u\\n\",shake_sprite_owner_wrapper_is_end,regs.sp,shake_sprite_owner_wrapper_return_sp,regs.e); exit(3); } uint16_t c_sp=combined_read16(S_c_sp,0); uint8_t depth=shake_read(S__foregroundVeraDepth); if(memory_get_ram_bank()!=shake_sprite_owner_wrapper_ram_bank || memory_get_rom_bank()!=shake_sprite_owner_wrapper_rom_bank || regs.k!=shake_sprite_owner_wrapper_code_bank || c_sp!=shake_sprite_owner_wrapper_c_sp || ((regs.status^shake_sprite_owner_wrapper_status)&FLAG_INTERRUPT) || depth!=shake_sprite_owner_wrapper_result_depth) { fprintf(stderr,\"Sprite-owner wrapper return contract failed: end=%u banks=%u/%u=>%u/%u codebank=%u=>%u Cstack=%04x=>%04x depth=%u=>%u expected=%u I=%u=>%u\\n\",shake_sprite_owner_wrapper_is_end,shake_sprite_owner_wrapper_ram_bank,shake_sprite_owner_wrapper_rom_bank,memory_get_ram_bank(),memory_get_rom_bank(),shake_sprite_owner_wrapper_code_bank,regs.k,shake_sprite_owner_wrapper_c_sp,c_sp,shake_sprite_owner_wrapper_expected_depth,depth,shake_sprite_owner_wrapper_result_depth,(shake_sprite_owner_wrapper_status&FLAG_INTERRUPT)!=0,(regs.status&FLAG_INTERRUPT)!=0); exit(3); } shake_check_outer_owner_interval(shake_sprite_owner_wrapper_is_end); regs.a=shake_sprite_owner_wrapper_a; regs.xl=shake_sprite_owner_wrapper_x; regs.yl=shake_sprite_owner_wrapper_y; regs.status=shake_sprite_owner_wrapper_status; shake_sprite_owner_wrapper_active=false; if(shake_sprite_owner_wrapper_is_end) { shake_view_warm_outer_owner_active=false; shake_view_warm_outer_owner_end_completed=true; fprintf(stderr,\"Released the real outer foreground owner after the changed sprite snapshot was complete\\n\"); } else { shake_view_warm_outer_owner_active=true; shake_view_warm_outer_owner_begin_completed=true; fprintf(stderr,\"Held the real outer foreground owner across both native sprite update passes\\n\"); } }\n"
"static void view_load_select_cold_view(void);\n"
"static void shake_maybe_schedule_sprite_owner_release(WarmSpriteProducerCall *call) { if(shake_view_warm_outer_owner_active && !shake_view_warm_outer_owner_end_scheduled && shake_view_warm_position_entries>=2 && shake_view_warm_latest_snapshot_saved && shake_view_warm_coordinates_changed) { shake_view_warm_outer_owner_release_pc=call->return_pc; shake_view_warm_outer_owner_release_sp=call->return_sp; shake_view_warm_outer_owner_release_pending=true; } }\n"
"static void shake_begin_view_warm(void) {\n"
"    static const uint8_t code[]={3,10,0,3,0,2,12,9,107,120,24,0,25,0,26,30,33,33,1,41,1,33,37,1,10,10,35,1};\n"
"    shake_view_warm_script_length=shake_view_warm_first_script_length=sizeof(code);\n"
"    shake_saved_var10=shake_banked_read(shake_var_address(10),0);\n"
"    shake_saved_var0=shake_banked_read(shake_var_address(0),0);\n"
"    shake_saved_flag9=shake_banked_read(shake_flag_address(9),0);\n"
"    shake_start_return_queued=shake_start_return_seen=shake_start_poll_active=false;\n"
"    view_load_select_cold_view();\n"
"    for(int i=0;i<50;i++) shake_saved_window[i]=shake_read(S_codeWindow+i);\n"
"    for(int i=0;i<2;i++) { shake_saved_cursor[i]=shake_read(S_cwCurrentCode+i); shake_saved_pointer[i]=shake_read(0xa9+i); shake_ram(S_cwCurrentCode+i,0); }\n"
"    for(int i=0;i<2;i++) shake_saved_end[i]=shake_read(S_endPos+i);\n"
"    shake_saved_code_window_invalid=shake_read(S_codeWindowInvalid); shake_ram(S_codeWindowInvalid,0);\n"
"    uint16_t code_base=(uint16_t)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8)),test_end=(uint16_t)(code_base+49);\n"
"    shake_ram(S_endPos,(uint8_t)test_end); shake_ram(S_endPos+1,(uint8_t)(test_end>>8));\n"
"    for(unsigned i=0;i<sizeof(code);i++) { shake_ram(S_codeWindow+i,code[i]); } shake_ram(S_codeWindow+sizeof(code),0);\n"
"    shake_view_warm_total_b9_entries=shake_view_warm_b9_since_handler=shake_view_warm_last_batch_b9_entries=shake_view_warm_sprite_handler_entries=0; shake_view_warm_b9_entries=shake_view_warm_call_count=shake_view_warm_active_call=shake_view_warm_last_batch_start=shake_view_warm_outer_owner_b9_entries=0; shake_view_warm_stale_entry_seen=shake_view_warm_second_script_injected=shake_view_warm_second_entry_pending=shake_view_warm_second_batch_consumed=shake_view_warm_first_snapshot_saved=shake_view_warm_latest_snapshot_saved=shake_view_warm_coordinates_changed=shake_view_warm_irq_input_latest=false; shake_view_warm_first_snapshot_length=shake_view_warm_latest_snapshot_length=0; shake_view_warm_script_complete=false;\n"
"    shake_view_warm_outer_owner_active=shake_view_warm_outer_owner_begin_scheduled=shake_view_warm_outer_owner_begin_completed=shake_view_warm_outer_owner_end_scheduled=shake_view_warm_outer_owner_end_completed=shake_view_warm_outer_owner_release_pending=false; shake_view_warm_outer_owner_release_pc=0; shake_view_warm_outer_owner_release_sp=0; shake_sprite_owner_wrapper_active=false; shake_sprite_owner_wrapper_progress_logged=false; shake_sprite_owner_wrapper_start_cycle=0; shake_view_warm_progress_logged=false; shake_view_warm_outer_frames_start=shake_view_warm_outer_service_start=0; shake_view_warm_outer_handlers_start=0;\n"
"    shake_view_warm_started=shake_view_warm_active=true; shake_view_warm_start_cycle=clockticks6502;\n"
"    fprintf(stderr,\"Warming real KQ1 picture, sprite, sound, graphics/status, and input paths with AGI commands\\n\");\n"
"}\n"
"static bool shake_logic0_interpreter_ready(uint8_t *loaded,uint16_t *code,uint16_t *end) { *loaded=shake_banked_read(S__logics,5); *code=(uint16_t)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8)); *end=(uint16_t)(shake_read(S_endPos)|((uint16_t)shake_read(S_endPos+1)<<8)); return *loaded&&*code>=0xa000&&*code<0xc000&&*end>*code&&*end<=0xc000; }\n"
"static void shake_finish_view_warm(void) {\n"
"    if(!shake_view_warm_outer_owner_begin_completed||!shake_view_warm_outer_owner_end_completed||shake_view_warm_outer_owner_active||shake_read(S__foregroundVeraDepth)) { fprintf(stderr,\"Warm sprite publication did not release its real nested foreground owner: begin=%u end=%u active=%u depth=%u\\n\",shake_view_warm_outer_owner_begin_completed,shake_view_warm_outer_owner_end_completed,shake_view_warm_outer_owner_active,shake_read(S__foregroundVeraDepth)); exit(3); }\n"
"    if(shake_view_warm_outer_owner_b9_entries<2) { fprintf(stderr,\"The real outer VERA owner did not span two producer passes: b9_calls=%u\\n\",shake_view_warm_outer_owner_b9_entries); exit(3); }\n"
"    unsigned records=(shake_sprite_batch_length-SHAKE_SPRITE_TERMINATOR_BYTES)/SHAKE_SPRITE_UPDATE_RECORD_BYTES; if(shake_view_warm_load_entries!=1||shake_view_warm_animate_entries!=1||shake_view_warm_set_entries!=1||shake_view_warm_position_entries!=2||shake_view_warm_draw_entries!=2||!shake_sprite_batch_saved||!shake_sprite_batch_processed||!records||shake_view_warm_b9_entries<2||!shake_view_warm_stale_entry_seen||!shake_view_warm_second_entry_pending||!shake_view_warm_first_snapshot_saved||!shake_view_warm_latest_snapshot_saved||!shake_view_warm_coordinates_changed||!shake_view_warm_irq_input_latest||!shake_view_warm_second_batch_consumed) { fprintf(stderr,\"KQ1 sprite snapshot warm-up did not consume the newest changed coordinates: commands=%u/%u/%u/%u/%u b9=%u pending=%u/%u snapshots=%u/%u changed=%u latest_input=%u latest_output=%u records=%u\\n\",shake_view_warm_load_entries,shake_view_warm_animate_entries,shake_view_warm_set_entries,shake_view_warm_position_entries,shake_view_warm_draw_entries,shake_view_warm_b9_entries,shake_view_warm_stale_entry_seen,shake_view_warm_second_entry_pending,shake_view_warm_first_snapshot_saved,shake_view_warm_latest_snapshot_saved,shake_view_warm_coordinates_changed,shake_view_warm_irq_input_latest,shake_view_warm_second_batch_consumed,records); exit(3); }\n"
"    if(shake_view_warm_call_count!=shake_view_warm_total_b9_entries||shake_view_warm_active_call) { fprintf(stderr,\"Warm sprite producer call trace is incomplete: calls=%u entries=%u active_call=%u\\n\",shake_view_warm_call_count,shake_view_warm_total_b9_entries,shake_view_warm_active_call); exit(3); }\n"
"    for(unsigned i=0;i<shake_view_warm_call_count;i++) { WarmSpriteProducerCall *call=&shake_view_warm_calls[i]; if(!call->complete||call->entry_depth!=call->exit_depth) { fprintf(stderr,\"Warm b9UpdateObjects call %u did not return at its entry ownership depth: pointers=%04x/%04x ownerDepth=%u/%u complete=%u\\n\",i,call->entry_pointer,call->exit_pointer,call->entry_depth,call->exit_depth,call->complete); exit(3); } fprintf(stderr,\"Warm sprite producer b9[%u] cycles=%u..%u pointer=%04x->%04x ownerDepth=%u->%u\\n\",i,call->entry_cycle,call->exit_cycle,call->entry_pointer,call->exit_pointer,call->entry_depth,call->exit_depth); }\n"
"    uint16_t tab=(uint16_t)(S__viewtab+shake_read(S__sizeOfViewTab)); uint8_t view=shake_banked_read(tab+shake_read(S__offsetOfCurrentView),9); if(view!=view_load_target||!shake_banked_read(S__loadedViews+view*shake_read(S__sizeOfView),0x11)) { fprintf(stderr,\"KQ1 sprite warm-up did not leave VIEW %u loaded on object 1 (found view %u)\\n\",view_load_target,view); exit(3); }\n"
"    for(int i=0;i<50;i++) shake_ram(S_codeWindow+i,shake_saved_window[i]);\n"
"    for(int i=0;i<2;i++) { shake_ram(S_cwCurrentCode+i,shake_saved_cursor[i]); shake_ram(0xa9+i,shake_saved_pointer[i]); shake_ram(S_endPos+i,shake_saved_end[i]); }\n"
"    shake_ram(S_codeWindowInvalid,shake_saved_code_window_invalid);\n"
"    for(int i=0;i<50;i++) if(shake_read(S_codeWindow+i)!=shake_saved_window[i]) { fprintf(stderr,\"KQ1 sprite warm-up did not restore code window\\n\"); exit(3); }\n"
"    for(int i=0;i<2;i++) if(shake_read(S_cwCurrentCode+i)!=shake_saved_cursor[i]||shake_read(0xa9+i)!=shake_saved_pointer[i]||shake_read(S_endPos+i)!=shake_saved_end[i]) { fprintf(stderr,\"KQ1 sprite warm-up did not restore interpreter pointers\\n\"); exit(3); }\n"
"    if(shake_read(S_codeWindowInvalid)!=shake_saved_code_window_invalid) { fprintf(stderr,\"KQ1 sprite warm-up did not restore code-window validity\\n\"); exit(3); }\n"
"    if(!shake_read(S__inputLineDisplayed)) { fprintf(stderr,\"Real accept.input warm-up command did not enable the input line\\n\"); exit(3); }\n"
"    shake_banked_ram(shake_var_address(10),0,shake_saved_var10);\n"
"    shake_banked_ram(shake_var_address(0),0,shake_saved_var0);\n"
"    fprintf(stderr,\"KQ1 real sprite producer warm-up passed: view=%u b9UpdateObjects=%u records=%u; latest changed coordinates reached VERA\\n\",view_load_target,shake_view_warm_b9_entries,records);\n"
"    static char start_return[] = \"\\r\"; machine_paste(start_return,false); shake_start_return_queued=true; fprintf(stderr,\"Queued a real Return after enabling KQ1 graphics/input for the foreground scene\\n\");\n"
"    shake_view_warm_active=false; shake_view_warm_finished=true;\n"
"}\n"
"static void loader_read_published(uint8_t *out) { unsigned n=0; uint16_t fields[]={S__numWords,S__numSynonyms,S__wordsData,S__wordPointers,S__synonymsList}; for(unsigned field=0;field<5;field++) { out[n++]=shake_banked_read(fields[field],0x12); out[n++]=shake_banked_read(fields[field]+1,0x12); } out[n++]=shake_banked_read(S__wordsDataBank,0x12); out[n++]=shake_banked_read(S__wordsPointersBank,0x12); out[n++]=shake_banked_read(S__synonymsListBank,0x12); out[n++]=shake_banked_read(S__wordBank,0x12); }\n"
"static void loader_read_words_text(uint8_t *out) { for(unsigned i=0;i<sizeof(loader_words_text_before);i++) out[i]=shake_banked_read(S__wordsTextStart+i,0x12); }\n"
"static unsigned loader_count_allocations(const uint8_t *allocation) {\n"
"unsigned count=0;\n"
"for(unsigned i=0;i<sizeof(loader_allocator_before);i++) { if(allocation[i]) count++; }\n"
"return count;\n"
"}\n"
"static void loader_observe_io(uint16_t pc,uint8_t bank) { if(!loader_words_active) return; if(bank==0x0f&&pc==S__b0FReadWordsPass) { loader_pass_entries++; loader_pass_active=true; loader_pass_return_pc=shake_call_return(&loader_pass_return_sp); } if(pc==S__cbm_read&&loader_pass_active) { loader_read_active=true; loader_read_return_pc=shake_call_return(&loader_read_return_sp); } if(loader_read_active&&pc==loader_read_return_pc&&(uint8_t)regs.sp==loader_read_return_sp) { loader_read_active=false; loader_read_calls++; loader_read_result=regs.a; loader_read_result_x=regs.xl; loader_read_bytes+=(unsigned)regs.a+((unsigned)regs.xl<<8); } if(bank==6&&pc==S__b6Cx16_fseek&&loader_pass_active) { loader_seek_active=true; loader_seek_return_pc=shake_call_return(&loader_seek_return_sp); } if(loader_seek_active&&pc==loader_seek_return_pc&&(uint8_t)regs.sp==loader_seek_return_sp) { loader_seek_active=false; loader_seek_calls++; loader_seek_result=regs.a; loader_seek_result_x=regs.xl; } if(loader_pass_active&&pc==loader_pass_return_pc&&(uint8_t)regs.sp==loader_pass_return_sp) { loader_pass_active=false; loader_pass_returns++; loader_pass_result=regs.a; } }\n"
"static void loader_finish(void) { uint8_t published[14]={0},allocator[232]={0},words_text[4500]={0}; unsigned allocation_added=0,allocation_lost=0; if(loader_words_entries) { loader_read_published(published); loader_read_words_text(words_text); for(unsigned i=0;i<sizeof(allocator);i++) allocator[i]=shake_banked_read(S__allocationArray+i,0x10); for(unsigned i=0;i<sizeof(allocator);i++) { if(!loader_allocator_before[i]&&allocator[i]) allocation_added++; if(loader_allocator_before[i]&&!allocator[i]) allocation_lost++; } } FILE *f=fopen(LOADER_RESULT_PATH,\"w\"); if(!f) { perror(LOADER_RESULT_PATH); exit(2); } fprintf(f,\"%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\",loader_init_entries,loader_init_returned,loader_init_result,loader_words_entries,loader_words_returned,loader_words_result,loader_open_entries,loader_truncated,loader_alloc_entries,memcmp(published,loader_published_before,sizeof(published))==0,memcmp(words_text,loader_words_text_before,sizeof(words_text))==0,memcmp(allocator,loader_allocator_before,sizeof(allocator))==0,loader_event_entries,loader_interpreter_entries,loader_irq_entries,loader_object_entries,loader_pass_entries,loader_pass_returns,loader_pass_result,loader_read_calls,loader_read_bytes,loader_read_result,loader_read_result_x,loader_seek_calls,loader_seek_result,loader_seek_result_x); fprintf(f,\" %u %u %u %u %u %u\",combined_read16(S__numWords,0x12),loader_search_count,loader_allocator_before_count,loader_count_allocations(allocator),allocation_added,allocation_lost); for(unsigned i=0;i<LOADER_SEARCH_COUNT;i++) fprintf(f,\" %d\",loader_search_results[i]); fprintf(f,\" %u %u %u %u\",loader_exit_seen,loader_exit_status,loader_exit_ram_bank,loader_exit_code_bank); fputc('\\n',f); if(fclose(f)) { perror(LOADER_RESULT_PATH); exit(2); } emu_write(5,0); raise(SIGINT); }\n"
"#if LOADER_TEST_ENABLED\n"
"static const char *loader_search_queries[LOADER_SEARCH_COUNT]={\"ant\",\"ante\",\"anthill\",\"anther\",\"aaa\",\"ajw\",\"ats\",\"aabx\",\"zzz\"};\n"
"static void loader_start_search(unsigned index) { if(index>=LOADER_SEARCH_COUNT||loader_search_active) { fprintf(stderr,\"Invalid WORDS search fixture state\\n\"); exit(3); } loader_search_index=index; loader_search_return_pc=regs.pc; loader_search_return_sp=regs.sp; loader_search_return_c_sp=combined_read16(S_c_sp,0); loader_search_return_ram_bank=memory_get_ram_bank(); loader_search_return_rom_bank=memory_get_rom_bank(); loader_search_return_code_bank=regs.k; loader_search_return_status=regs.status; loader_search_return_a=regs.a; loader_search_return_x=regs.xl; loader_search_return_y=regs.yl; loader_search_return_e=regs.e; if(!regs.e||(regs.sp&0xff00)!=0x0100) { fprintf(stderr,\"WORDS target search requires emulation mode and the full page-one hardware stack: E=%u SP=%04x\\n\",regs.e,regs.sp); exit(3); } for(unsigned i=0;i<41;i++) shake_banked_ram(S__testWordsQuery+i,0,0); for(unsigned i=0;loader_search_queries[index][i];i++) shake_banked_ram(S__testWordsQuery+i,0,(uint8_t)loader_search_queries[index][i]); loader_search_active=true; shake_push_test_jsr(S__testRunWordsSearch); }\n"
"static void loader_search_return(uint16_t pc) { if(!loader_search_active||pc!=loader_search_return_pc) return; if(regs.sp!=loader_search_return_sp||(regs.sp&0xff00)!=0x0100||regs.e!=loader_search_return_e||memory_get_ram_bank()!=loader_search_return_ram_bank||memory_get_rom_bank()!=loader_search_return_rom_bank||regs.k!=loader_search_return_code_bank||combined_read16(S_c_sp,0)!=loader_search_return_c_sp||((regs.status^loader_search_return_status)&FLAG_INTERRUPT)) { fprintf(stderr,\"WORDS target search failed to restore caller state: SP=%04x/%04x E=%u/%u banks=%u/%u expected=%u/%u code=%u/%u Cstack=%04x/%04x I=%u/%u\\n\",regs.sp,loader_search_return_sp,regs.e,loader_search_return_e,memory_get_ram_bank(),memory_get_rom_bank(),loader_search_return_ram_bank,loader_search_return_rom_bank,regs.k,loader_search_return_code_bank,combined_read16(S_c_sp,0),loader_search_return_c_sp,(regs.status&FLAG_INTERRUPT)!=0,(loader_search_return_status&FLAG_INTERRUPT)!=0); exit(3); } loader_search_results[loader_search_count++]=(int16_t)combined_read16(S__testWordsResult,0); fprintf(stderr,\"WORDS search %s => %d\\n\",loader_search_queries[loader_search_index],loader_search_results[loader_search_index]); regs.a=loader_search_return_a; regs.xl=loader_search_return_x; regs.yl=loader_search_return_y; regs.status=loader_search_return_status; loader_search_active=false; if(loader_search_count<LOADER_SEARCH_COUNT) loader_start_search(loader_search_count); else loader_finish(); }\n"
"#endif\n"
"static void loader_before_step(uint16_t pc,uint8_t bank) {\n"
"if(waiting) { return; }\n"
"#if LOADER_TEST_ENABLED\n"
"if(loader_search_active) { if(pc==loader_search_return_pc) { loader_search_return(pc); return; } }\n"
"#endif\n"
"if(regs.k) { return; }\n"
"if(loader_init_active&&regs.k==0&&pc==S__exit) { loader_exit_seen++; loader_exit_status=(uint16_t)regs.a|((uint16_t)regs.xl<<8); loader_exit_ram_bank=bank; loader_exit_code_bank=regs.k; fprintf(stderr,\"loader resident _exit entry: PC=%04x RAM=%02x codebank=%02x status=%04x\\n\",pc,bank,regs.k,loader_exit_status); loader_finish(); return; } loader_observe_io(pc,bank); if(bank==6&&pc==S__b6Initialise) { loader_init_entries++; loader_init_active=true; loader_init_return_pc=shake_call_return(&loader_init_return_sp); } if(bank==6&&pc==S__b6InitInterpreter) loader_interpreter_entries++; if(bank==6&&pc==S__b6InitIrq) loader_irq_entries++; if(bank==7&&pc==S__b7InitEvents) loader_event_entries++; if(bank==0x0d&&pc==S__bDLoadObjectFile) loader_object_entries++; if(loader_init_active&&bank==0x12&&pc==S__b12LoadWords) { loader_words_entries++; loader_words_active=true; loader_words_return_pc=shake_call_return(&loader_words_return_sp); loader_read_published(loader_published_before); loader_read_words_text(loader_words_text_before); for(unsigned i=0;i<sizeof(loader_allocator_before);i++) loader_allocator_before[i]=shake_banked_read(S__allocationArray+i,0x10); loader_allocator_before_count=loader_count_allocations(loader_allocator_before); } if(loader_words_active&&bank==0x10&&pc==S__b10BankedAlloc) loader_alloc_entries++; if(loader_words_active&&bank==0x12&&pc==S__b12OpenWords) { loader_open_entries++; loader_open_active=true; loader_open_return_pc=shake_call_return(&loader_open_return_sp); if(loader_open_entries==2&&access(\"loader-staging-read.marker\",F_OK)==0) { FILE *f=fopen(LOADER_WORDS_PATH,\"r+b\"); if(!f||fseek(f,0,SEEK_END)) { perror(LOADER_WORDS_PATH); exit(2); } long size=ftell(f); fclose(f); if(size<=2||truncate(LOADER_WORDS_PATH,size-2)) { perror(LOADER_WORDS_PATH); exit(2); } loader_truncated=true; } } if(loader_open_active&&pc==loader_open_return_pc&&(uint8_t)regs.sp==loader_open_return_sp) loader_open_active=false; if(loader_words_active&&pc==loader_words_return_pc&&(uint8_t)regs.sp==loader_words_return_sp) { loader_words_result=regs.a; loader_words_returned=true; loader_words_active=false; } if(loader_init_active&&pc==loader_init_return_pc&&(uint8_t)regs.sp==loader_init_return_sp) { loader_init_result=regs.a; loader_init_returned=true; loader_init_active=false;\n"
"#if LOADER_TEST_ENABLED\n"
"if(loader_init_result&&loader_words_result) { loader_start_search(0); return; }\n"
"#endif\n"
"loader_finish(); } }\n"
"static void view_load_select_cold_view(void) { uint16_t count=blit_read16_banked(S__numViews,0); if(count<=33) { fprintf(stderr,\"KQ1 multi-slot fixture requires VIEW 33, directory count is %u\\n\",count); exit(3); } uint16_t row=(uint16_t)(S__loadedViews+33u*shake_read(S__sizeOfView)); uint16_t dir=(uint16_t)(S__viewdir+33u*5u); uint32_t position=blit_read32_banked(dir+1,0x10); uint8_t file=shake_banked_read(dir,0x10); if(shake_banked_read(row,0x11)||file>=16||position>0xfffffu||position==0xfffffu) { fprintf(stderr,\"KQ1 multi-slot VIEW 33 must be valid and cold (loaded=%u file=%u position=%05lx)\\n\",shake_banked_read(row,0x11),file,(unsigned long)position); exit(3); } view_load_target=33; }\n"
"static void view_load_test_begin(void) { view_load_select_cold_view(); view_load_failure_armed=true; view_load_failure_active=view_load_failure_returned=view_load_loader_active=view_load_loader_returned=view_load_retry_loader_returned=false; view_load_failure_command_entries=view_load_failure_caller_entries=view_load_failure_loader_entries=view_load_retry_caller_entries=view_load_retry_loader_entries=0; const uint8_t code[]={30,view_load_target}; picture_save_injection(code,sizeof(code)); picture_test_stage=4; }\n"
"static void view_load_snapshot_failure_state(void) { view_load_view_size=shake_read(S__sizeOfView); view_load_metadata_size=(uint8_t)(shake_read(S__offsetOfBackBuffers)+4); if(!view_load_view_size||view_load_view_size>sizeof(view_load_before_view)||!view_load_metadata_size||view_load_metadata_size>sizeof(view_load_before_metadata[0])) { fprintf(stderr,\"Invalid view or metadata record size\\n\"); exit(3); } uint16_t view=(uint16_t)(S__loadedViews+view_load_target*view_load_view_size); if(shake_banked_read(view,0x11)) { fprintf(stderr,\"Selected view %u is not cold at load entry\\n\",view_load_target); exit(3); } for(unsigned i=0;i<view_load_view_size;i++) view_load_before_view[i]=shake_banked_read(view+i,0x11); for(unsigned entry=0;entry<20;entry++) for(unsigned i=0;i<view_load_metadata_size;i++) view_load_before_metadata[entry][i]=shake_banked_read(S__viewTableMetadata+entry*view_load_metadata_size+i,14); for(unsigned i=0;i<2160;i++) view_load_before_allocator[i]=shake_banked_read(S__bDSpriteAllocTable+i,13); if(shake_banked_read(S__bDSpriteAllocTable+2160,13)!=0xff) { fprintf(stderr,\"Sprite allocator terminator is invalid before failed view load\\n\"); exit(3); } }\n"
"static void view_load_failure_before_step(uint16_t pc,uint8_t bank) { if(!PICTURE_TEST_ENABLED||regs.k||waiting) return; if(picture_test_stage==4&&view_load_failure_armed&&bank==9&&pc==S__b9LoadViewFile&&regs.a==view_load_target) { view_load_failure_armed=false; view_load_failure_active=true; view_load_failure_caller_entries++; view_load_failure_return_pc=shake_call_return(&view_load_failure_return_sp); view_load_snapshot_failure_state(); uint16_t dir=(uint16_t)(S__viewdir+view_load_target*5u); for(unsigned i=0;i<5;i++) view_load_directory[i]=shake_banked_read(dir+i,0x10); for(unsigned i=1;i<5;i++) shake_banked_ram(dir+i,0x10,0xff); } if(picture_test_stage==5&&bank==9&&pc==S__b9LoadViewFile&&regs.a==view_load_target) view_load_retry_caller_entries++; if(picture_test_stage==4&&view_load_failure_active&&!view_load_loader_active&&bank==6&&pc==S__b6LoadAGIFile) { view_load_failure_loader_entries++; view_load_loader_active=true; view_load_loader_return_pc=shake_call_return(&view_load_loader_return_sp); } else if(picture_test_stage==5&&bank==6&&pc==S__b6LoadAGIFile&&!view_load_loader_active) { view_load_retry_loader_entries++; view_load_loader_active=true; view_load_loader_return_pc=shake_call_return(&view_load_loader_return_sp); } if(view_load_loader_active&&pc==view_load_loader_return_pc&&(uint8_t)regs.sp==view_load_loader_return_sp) { if(picture_test_stage==4) { view_load_loader_result=regs.a; view_load_loader_returned=true; } else if(picture_test_stage==5) { view_load_retry_loader_result=regs.a; view_load_retry_loader_returned=true; } view_load_loader_active=false; } if(view_load_failure_active&&pc==view_load_failure_return_pc&&(uint8_t)regs.sp==view_load_failure_return_sp) { uint16_t dir=(uint16_t)(S__viewdir+view_load_target*5u); for(unsigned i=0;i<5;i++) shake_banked_ram(dir+i,0x10,view_load_directory[i]); uint16_t view=(uint16_t)(S__loadedViews+view_load_target*view_load_view_size); view_load_view_unchanged=true; for(unsigned i=0;i<view_load_view_size;i++) if(shake_banked_read(view+i,0x11)!=view_load_before_view[i]) view_load_view_unchanged=false; view_load_metadata_unchanged=true; for(unsigned entry=0;entry<20;entry++) for(unsigned i=0;i<view_load_metadata_size;i++) if(shake_banked_read(S__viewTableMetadata+entry*view_load_metadata_size+i,14)!=view_load_before_metadata[entry][i]) view_load_metadata_unchanged=false; view_load_allocator_unchanged=shake_banked_read(S__bDSpriteAllocTable+2160,13)==0xff; for(unsigned i=0;i<2160;i++) if(shake_banked_read(S__bDSpriteAllocTable+i,13)!=view_load_before_allocator[i]) view_load_allocator_unchanged=false; view_load_failure_active=false; view_load_failure_returned=true; } }\n"
"static void view_load_retry_before_step(uint16_t pc,uint8_t bank) { (void)bank; if(!PICTURE_TEST_ENABLED||picture_test_stage!=6||regs.k||waiting) return; if(!view_load_retry_observer_active&&!view_load_retry_blit_done&&blit_failure_stage==3&&pc==S__agiBlit&&blit_current_entry_is_target()&&shake_read(S__foregroundVeraDepth)==1) { view_load_retry_observer_active=true; view_load_retry_entry_bank=memory_get_ram_bank(); view_load_retry_entry_rom=memory_get_rom_bank(); view_load_retry_entry_i=(regs.status&FLAG_INTERRUPT)!=0; view_load_retry_entry_depth=shake_read(S__foregroundVeraDepth); view_load_retry_blit_return_pc=shake_call_return(&view_load_retry_blit_return_sp); return; } if(view_load_retry_observer_active&&pc==view_load_retry_blit_return_pc&&(uint8_t)regs.sp==view_load_retry_blit_return_sp) { view_load_retry_blit_result=regs.a; view_load_retry_bank_preserved=memory_get_ram_bank()==view_load_retry_entry_bank; view_load_retry_rom_preserved=memory_get_rom_bank()==view_load_retry_entry_rom; view_load_retry_i_preserved=((regs.status&FLAG_INTERRUPT)!=0)==view_load_retry_entry_i; view_load_retry_depth_restored=view_load_retry_entry_depth==1&&shake_read(S__foregroundVeraDepth)==view_load_retry_entry_depth; uint16_t tab=(uint16_t)(S__viewtab+shake_read(S__sizeOfViewTab)); uint8_t current_view=shake_banked_read(tab+shake_read(S__offsetOfCurrentView),9); uint16_t loaded=(uint16_t)(S__loadedViews+view_load_target*shake_read(S__sizeOfView)); uint8_t slots=shake_banked_read(loaded+shake_read(S__offsetOfMaxVeraSlots),0x11); BlitMetadataSnapshot retry_meta=blit_capture_metadata(1); uint8_t expected_backbuffer_size=(uint8_t)((8u<<retry_meta.width)+(8u<<retry_meta.height)); uint8_t retry_metadata_committed=retry_meta.is_on_backbuffer==1&&retry_meta.backbuffer_size==expected_backbuffer_size; view_load_retry_loaded_view=shake_banked_read(loaded,0x11)!=0&&current_view==view_load_target&&slots>=2; if(!view_load_failure_returned||!view_load_loader_returned||view_load_loader_result||!view_load_view_unchanged||!view_load_metadata_unchanged||!view_load_allocator_unchanged||view_load_retry_caller_entries!=1||view_load_retry_loader_entries!=1||!view_load_retry_loader_returned||!view_load_retry_loader_result||!view_load_retry_loaded_view||!view_load_retry_blit_result||!retry_meta.loop_valid||!retry_meta.loop_reserved||!retry_meta.backbuffer_valid||!retry_meta.backbuffer_reserved||!retry_metadata_committed||!view_load_retry_bank_preserved||!view_load_retry_rom_preserved||!view_load_retry_i_preserved||!view_load_retry_depth_restored) { fprintf(stderr,\"VIEW 33 caller failure/retry check failed: failure=%u loader=%u/%u row/meta/map=%u/%u/%u retry=%u/%u loaded/slots=%u/%u blit=%u bank/rom/i/depth=%u/%u/%u/%u\\n\",view_load_failure_returned,view_load_loader_returned,view_load_loader_result,view_load_view_unchanged,view_load_metadata_unchanged,view_load_allocator_unchanged,view_load_retry_caller_entries,view_load_retry_loader_entries,view_load_retry_loaded_view,slots,view_load_retry_blit_result,view_load_retry_bank_preserved,view_load_retry_rom_preserved,view_load_retry_i_preserved,view_load_retry_depth_restored); exit(3); } view_load_retry_blit_done=true; unsigned values[]={ view_load_target,view_load_failure_command_entries,view_load_failure_caller_entries,view_load_failure_loader_entries,view_load_loader_returned,!view_load_loader_result,view_load_failure_returned,view_load_view_unchanged,view_load_metadata_unchanged,view_load_allocator_unchanged,blit_load_view_entries,blit_set_view_entries,blit_draw_entries,view_load_retry_caller_entries,view_load_retry_loader_entries,view_load_retry_loader_returned,view_load_retry_loader_result!=0,view_load_retry_loaded_view,view_load_retry_blit_result,view_load_retry_bank_preserved,view_load_retry_rom_preserved,view_load_retry_i_preserved,view_load_retry_depth_restored,retry_meta.loop_valid,retry_meta.loop_reserved,retry_meta.backbuffer_valid,retry_meta.backbuffer_reserved,retry_metadata_committed }; if(!view_load_retry_csv) { view_load_retry_csv=fopen(VIEW_LOAD_RETRY_CSV,\"w\"); if(!view_load_retry_csv) { perror(VIEW_LOAD_RETRY_CSV); exit(2); } fprintf(view_load_retry_csv,\"view_num,failure_load_command,failure_b9_entries,failure_loader_entries,failure_loader_returned,failure_loader_false,failure_b9_returned,failure_view_unchanged,failure_metadata_unchanged,failure_allocator_unchanged,retry_load_command,retry_set_command,retry_draw_command,retry_b9_entries,retry_loader_entries,retry_loader_returned,retry_loader_true,retry_loaded_view,retry_agi_blit_result,retry_bank_preserved,retry_rom_preserved,retry_i_preserved,retry_depth_restored,retry_loop_valid,retry_loop_reserved,retry_backbuffer_valid,retry_backbuffer_reserved,retry_metadata_committed\\n\"); } for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++) fprintf(view_load_retry_csv,\"%s%u\",i?\",\":\"\",values[i]); fputc('\\n',view_load_retry_csv); fflush(view_load_retry_csv); fclose(view_load_retry_csv); view_load_retry_csv=NULL; view_load_retry_observer_active=false; } }\n"
"/* _agiBlit entry follows its caller's one-byte pusha(entryNum); return consumes it. */\n"
"static void blit_failure_before_step(uint16_t pc,uint8_t bank) {"
"    if(!PICTURE_TEST_ENABLED||picture_test_stage!=6||waiting||(regs.k&&!blit_fixture_call_active)) return;"
"    if(blit_fixture_call_active) {"
"        if(!blit_fixture_helper_entries&&(pc==(uint16_t)(blit_fixture_target+2)||pc==(uint16_t)(blit_fixture_target+3))&&regs.sp==(uint16_t)(blit_fixture_return_sp-2)) {"
"            blit_fixture_helper_entries++;"
"            if(blit_fixture_op==1||blit_fixture_op==2) blit_fixture_setup_entries++;"
"            else if(blit_fixture_op==3) blit_fixture_release_entries++;"
"        }"
"        if(bank==0x0d&&pc==S_bDFindFreeVramBlock) blit_fixture_find_entries++;"
"        if(bank==0x0d&&pc==S__bDDeleteAllocation) blit_fixture_delete_entries++;"
"        if(pc==blit_fixture_return_pc&&regs.sp==blit_fixture_return_sp) { uint8_t completed_op=blit_fixture_op; blit_fixture_finish_call(); if(completed_op!=3) return; }"
"        else return;"
"    }"
"    if(blit_pending_parent_phase&&blit_failure_stage==2) blit_allocator_trace_step(pc,bank);"
"    if(bank==9&&pc==S_b9UpdateObjects) {"
"        blit_target_entry_num=1;"
"        uint16_t tab=(uint16_t)(S__viewtab+blit_target_entry_num*shake_read(S__sizeOfViewTab));"
"        uint8_t flags_offset=shake_read(S__offsetOfFlags);"
"        blit_loaded_view_num=shake_banked_read((uint16_t)(tab+shake_read(S__offsetOfCurrentView)),9);"
"        uint8_t flags=shake_banked_read((uint16_t)(tab+flags_offset),9);"
"        blit_loaded_resource=shake_banked_read((uint16_t)(S__loadedViews+blit_loaded_view_num*shake_read(S__sizeOfView)),0x11)!=0;"
"        if(!blit_loaded_resource) { fprintf(stderr,\"Native load.view did not load resource %u\\n\",blit_loaded_view_num); exit(3); }"
"        if(!(flags&1)) shake_banked_ram((uint16_t)(tab+flags_offset),9,(uint8_t)(flags|1));"
"        uint16_t stale=(uint16_t)(tab+shake_read(S__offsetOfStopped)-1);"
"        if(!blit_saved_stale_counter) { blit_stale_counter=shake_banked_read(stale,9); blit_saved_stale_counter=true; }"
"        shake_banked_ram(stale,9,1);"
"        blit_snapshot_parent_viewtab();"
"    }"
"    if(blit_failure_stage==3&&foreground_active&&foreground_kind==2&&!blit_pending_parent_phase&&!blit_retry_active&&pc==S__agiBlit&&blit_current_entry_is_target()&&shake_read(S__foregroundVeraDepth)==1) {"
"        blit_before_retry=blit_capture_metadata(blit_target_entry_num); blit_retry_active=true; blit_retry_entered=1; blit_retry_set_loop_entries=0;"
"        blit_set_loop_entered=blit_second_allocator_entered=false; blit_entry_bank=bank; blit_entry_rom=memory_get_rom_bank(); blit_entry_i=(regs.status&FLAG_INTERRUPT)!=0;"
"        blit_entry_depth=shake_read(S__foregroundVeraDepth); blit_retry_entry_c_sp=combined_read16(S_c_sp,0); blit_nested_depth=blit_entry_depth;"
"        blit_retry_return_pc=shake_call_return(&blit_return_sp);"
"    }"
"    if(blit_retry_active) {"
"        uint8_t depth=shake_read(S__foregroundVeraDepth);"
"        if(depth>blit_nested_depth) blit_nested_depth=depth;"
"        if(bank==0x0e&&pc==S__bESetLoop) blit_retry_set_loop_entries++;"
"        if(pc==blit_retry_return_pc&&(uint8_t)regs.sp==blit_return_sp) {"
"            blit_result=regs.a; blit_retry_result=regs.a!=0; blit_agi_return_depth=shake_read(S__foregroundVeraDepth);"
"            blit_bank_preserved=memory_get_ram_bank()==blit_entry_bank; blit_rom_preserved=memory_get_rom_bank()==blit_entry_rom;"
"            blit_i_preserved=((regs.status&FLAG_INTERRUPT)!=0)==blit_entry_i; blit_retry_c_sp_preserved=combined_read16(S_c_sp,0)==(uint16_t)(blit_retry_entry_c_sp+1u);"
"            bool retry_viewcaller=blit_current_entry_is_target();"
"            blit_after_retry=blit_capture_metadata(blit_target_entry_num);"
"            blit_loop_addresses_unchanged=(blit_after_failure.count==blit_after_retry.count&&blit_same_records(blit_after_failure.loop,blit_after_retry.loop,blit_after_failure.count));"
"            bool old_loop_reservations=blit_snapshot_loop_ranges_reserved(&blit_after_failure);"
"            blit_retry_loop_consistent=blit_after_failure.count==blit_after_retry.count&&blit_after_failure.view_num==blit_after_retry.view_num&&blit_after_failure.metadata_bank==blit_after_retry.metadata_bank&&blit_retry_set_loop_entries==((blit_before_retry.count&&blit_before_retry.loop[0])?0u:1u)&&((blit_after_failure.loop_num==blit_after_retry.loop_num&&blit_after_failure.cel_num==blit_after_retry.cel_num&&blit_loop_addresses_unchanged)||((blit_after_failure.loop_num!=blit_after_retry.loop_num||blit_after_failure.cel_num!=blit_after_retry.cel_num)&&old_loop_reservations));"
"            fprintf(stderr,\"Retry target metadata view/loop/cel=%u/%u/%u -> %u/%u/%u; record=%04x->%04x before-slot0=%06x setLoop=%u expected=%u old-loop-reservations=%u current-loop-valid/reserved=%u/%u\\n\",blit_after_failure.view_num,blit_after_failure.loop_num,blit_after_failure.cel_num,blit_after_retry.view_num,blit_after_retry.loop_num,blit_after_retry.cel_num,blit_after_failure.loop_records,blit_after_retry.loop_records,(unsigned)blit_before_retry.loop[0],blit_retry_set_loop_entries,(blit_before_retry.count&&blit_before_retry.loop[0])?0u:1u,old_loop_reservations,blit_after_retry.loop_valid,blit_after_retry.loop_reserved);"
"            blit_loop_addresses_valid=blit_after_retry.loop_valid; blit_loop_addresses_reserved=blit_after_retry.loop_reserved;"
"            blit_backbuffers_unchanged=blit_same_backbuffer_metadata(&blit_after_failure,&blit_after_retry); blit_backbuffers_valid=blit_after_retry.backbuffer_valid;"
"            blit_backbuffers_reserved=blit_after_retry.backbuffer_reserved; blit_metadata_unchanged=blit_backbuffers_unchanged;"
"            blit_retry_backbuffers_valid=blit_backbuffers_valid; blit_retry_backbuffers_reserved=blit_backbuffers_reserved;"
"            uint8_t expected_size=(uint8_t)((8u<<blit_after_retry.width)+(8u<<blit_after_retry.height));"
"            blit_retry_metadata_committed=blit_after_retry.is_on_backbuffer==1&&blit_after_retry.backbuffer_size==expected_size;"
"            blit_retry_bank_preserved=blit_bank_preserved; blit_retry_rom_preserved=blit_rom_preserved; blit_retry_i_preserved=blit_i_preserved;"
"            blit_retry_depth_restored=(blit_entry_depth==1&&blit_agi_return_depth==blit_entry_depth);"
"            if(!blit_retry_c_sp_preserved||!retry_viewcaller) { fprintf(stderr,\"agiBlit retry changed caller state: Cstack=%u viewcaller=%u\\n\",blit_retry_c_sp_preserved,retry_viewcaller); exit(3); }"
"            blit_pending_parent_phase=true; blit_retry_active=false;"
"        }"
"        return;"
"    }"
"    if(blit_failure_stage<1||blit_failure_stage>2) return;"
"    if(foreground_active&&foreground_kind==2&&!blit_pending_parent_phase&&!blit_failure_active&&pc==S__agiBlit&&blit_current_entry_is_target()&&shake_read(S__foregroundVeraDepth)==1) {"
"        blit_failure_active=true;"
"        blit_set_loop_entered=blit_set_loop_returned=blit_second_allocator_entered=false;"
"        blit_allocator_trace_depth=blit_allocator_trace_calls=blit_allocator_trace_printed=blit_allocator_trace_overflow=0;"
"        blit_allocator_event_count=blit_allocator_event_overflow=0;"
"        blit_set_loop_succeeded=blit_set_loop_bank_restored=false;"
"        blit_bulk_helper_entered=blit_bulk_helper_returned=blit_c_allocator_returned=false;"
"        blit_partial_first_slot_seen=blit_partial_map_rolled_back=blit_partial_records_zero=blit_allocator_restored=false; blit_bulk_find_entries=0;"
"        blit_fixture_cleanup_started=blit_fixture_cleanup_done=false;"
"        blit_nested_depth=0; blit_entry_bank=bank; blit_entry_rom=memory_get_rom_bank(); blit_entry_i=(regs.status&FLAG_INTERRUPT)!=0;"
"        blit_entry_depth=shake_read(S__foregroundVeraDepth); blit_entry_c_sp=combined_read16(S_c_sp,0); blit_target_entry_num=1;"
"        blit_return_pc=shake_call_return(&blit_return_sp);"
"        blit_save_allocator(); blit_after_set_loop_state_saved=false;"
"        if(blit_failure_stage==1) { blit_fixture_begin(1,S__testReserveSpriteFixtureBlocks); return; }"
"        if(!blit_allocator_map_equals(blit_saved_allocator)) { fprintf(stderr,\"VIEW 33 stage-2 fixture changed the original allocator before bESetLoop\\n\"); exit(3); }"
"    }"
"    if(!blit_failure_active) return;"
"    if(bank==0x0e&&pc==S__bESetLoop&&!blit_set_loop_entered) {"
"        blit_set_loop_entered=true; blit_before_set_loop=blit_capture_metadata(blit_target_entry_num);"
"        blit_set_loop_entry_bank=bank; blit_set_loop_return_pc=shake_call_return(&blit_set_loop_return_sp);"
"        uint8_t depth=shake_read(S__foregroundVeraDepth); if(depth>blit_nested_depth) blit_nested_depth=depth;"
"    }"
"    if(blit_set_loop_entered&&!blit_set_loop_returned&&pc==blit_set_loop_return_pc&&(uint8_t)regs.sp==blit_set_loop_return_sp) {"
"        blit_set_loop_returned=true; blit_set_loop_succeeded=regs.a!=0; blit_set_loop_bank_restored=memory_get_ram_bank()==blit_set_loop_entry_bank;"
"    }"
"    if(bank==0x0e&&pc==S__bEAllocateSpriteMemory&&blit_failure_stage==2&&!blit_second_allocator_entered) {"
"        blit_second_allocator_entered=true; blit_c_allocator_return_pc=shake_call_return(&blit_c_allocator_return_sp);"
"        blit_save_allocator_state(blit_saved_after_set_loop_allocator,blit_saved_after_set_loop_code); blit_after_set_loop_state_saved=true;"
"        uint8_t depth=shake_read(S__foregroundVeraDepth); if(depth>blit_nested_depth) blit_nested_depth=depth;"
"        blit_fixture_begin(2,S__testPrepareSpriteFixturePartialRun); return;"
"    }"
"    if(bank==0x0e&&pc==S__bEAllocateSpriteMemoryBulk&&blit_second_allocator_entered&&!blit_bulk_helper_entered) {"
"        blit_bulk_helper_entered=true; blit_bulk_return_pc=shake_call_return(&blit_bulk_return_sp);"
"    }"
"    if(bank==0x0d&&pc==S_bDFindFreeVramBlock&&blit_bulk_helper_entered) {"
"        blit_bulk_find_entries++;"
"        if(blit_bulk_find_entries==2) blit_partial_first_slot_seen=blit_read32_banked(S__bEBulkAllocatedAddresses,0x0e)==blit_partial_expected_address&&blit_map_all_occupied();"
"    }"
"    if(blit_bulk_helper_entered&&!blit_bulk_helper_returned&&pc==blit_bulk_return_pc&&(uint8_t)regs.sp==blit_bulk_return_sp) {"
"        blit_bulk_helper_returned=true; blit_bulk_result=regs.a;"
"        blit_partial_map_rolled_back=blit_allocator_map_equals(blit_fixture_allocator_before);"
"        blit_partial_records_zero=blit_bulk_records_zero();"
"    }"
"    if(blit_second_allocator_entered&&!blit_c_allocator_returned&&pc==blit_c_allocator_return_pc&&(uint8_t)regs.sp==blit_c_allocator_return_sp) {"
"        blit_c_allocator_returned=true; blit_c_allocator_result=regs.a;"
"        if(!blit_bulk_helper_returned||blit_bulk_result||blit_bulk_find_entries!=2||!blit_partial_first_slot_seen||!blit_partial_map_rolled_back||!blit_partial_records_zero||blit_c_allocator_result||!blit_allocator_map_equals(blit_fixture_allocator_before)) {"
"            fprintf(stderr,\"C sprite allocator return failed: helper=%u/%u finds=%u first/map/records=%u/%u/%u result=%u\\n\",blit_bulk_helper_returned,blit_bulk_result,blit_bulk_find_entries,blit_partial_first_slot_seen,blit_partial_map_rolled_back,blit_partial_records_zero,blit_c_allocator_result); exit(3);"
"        }"
"    }"
"    if(pc==blit_return_pc&&(uint8_t)regs.sp==blit_return_sp) {"
"        if(!blit_fixture_cleanup_started) {"
"            blit_fixture_cleanup_started=true; blit_fixture_begin(3,S__testReleaseSpriteFixtureBlocks); return;"
"        }"
"        blit_result=regs.a; blit_agi_return_depth=shake_read(S__foregroundVeraDepth);"
"        blit_bank_preserved=memory_get_ram_bank()==blit_entry_bank; blit_rom_preserved=memory_get_rom_bank()==blit_entry_rom;"
"        blit_i_preserved=((regs.status&FLAG_INTERRUPT)!=0)==blit_entry_i;"
"        blit_failure_c_sp_preserved=combined_read16(S_c_sp,0)==(uint16_t)(blit_entry_c_sp+1u);"
"        bool failure_viewcaller=blit_current_entry_is_target();"
"        bool branch_ok=blit_failure_stage==1 ? (blit_set_loop_entered&&blit_set_loop_returned&&!blit_set_loop_succeeded&&blit_set_loop_bank_restored&&blit_fixture_setup_entries==1&&blit_fixture_release_entries==1&&blit_fixture_find_entries>0&&blit_fixture_delete_entries>0&&blit_fixture_owned_blocks==blit_fixture_released_blocks&&blit_fixture_cleanup_done&&blit_allocator_map_equals(blit_saved_allocator)) : (blit_second_allocator_entered&&blit_bulk_helper_entered&&blit_bulk_helper_returned&&!blit_bulk_result&&blit_bulk_find_entries==2&&blit_partial_first_slot_seen&&blit_partial_map_rolled_back&&blit_partial_records_zero&&blit_c_allocator_returned&&!blit_c_allocator_result&&blit_after_set_loop_state_saved&&blit_allocator_map_equals(blit_saved_after_set_loop_allocator)&&blit_allocator_restored&&blit_fixture_setup_entries==1&&blit_fixture_release_entries==1&&blit_fixture_find_entries>0&&blit_fixture_delete_entries>0&&blit_fixture_owned_blocks==blit_fixture_released_blocks&&blit_fixture_cleanup_done&&blit_bulk_records_zero());"
"        uint8_t expected_nested_depth=blit_entry_depth;"
"        if(!(blit_fixture_cleanup_done&&blit_entry_depth==1&&blit_nested_depth==expected_nested_depth&&branch_ok&&blit_result==0&&blit_agi_return_depth==blit_entry_depth&&blit_bank_preserved&&blit_rom_preserved&&blit_i_preserved&&blit_failure_c_sp_preserved&&failure_viewcaller)) {"
"            fprintf(stderr,\"agiBlit phase %d failed: depth %u->%u expected=%u->%u result=%u setloop=%u/%u/%u setbank=%u alloc=%u bulk=%u/%u finds=%u first/map/records=%u/%u/%u C=%u/%u cstack=%04x/%04x fixture=%u/%u/%u/%u/%u/%u caller=%u/%u bank/rom/i=%u/%u/%u\\n\",blit_failure_stage,blit_entry_depth,blit_nested_depth,expected_nested_depth,blit_agi_return_depth,blit_result,blit_set_loop_entered,blit_set_loop_returned,blit_set_loop_succeeded,blit_set_loop_bank_restored,blit_second_allocator_entered,blit_bulk_helper_entered,blit_bulk_helper_returned,blit_bulk_find_entries,blit_partial_first_slot_seen,blit_partial_map_rolled_back,blit_partial_records_zero,blit_c_allocator_returned,blit_c_allocator_result,combined_read16(S_c_sp,0),blit_entry_c_sp,blit_fixture_setup_entries,blit_fixture_find_entries,blit_fixture_delete_entries,blit_fixture_owned_blocks,blit_fixture_released_blocks,blit_fixture_cleanup_done,blit_failure_c_sp_preserved,failure_viewcaller,blit_bank_preserved,blit_rom_preserved,blit_i_preserved); exit(3);"
"        }"
"        blit_after_set_loop_state_saved=blit_failure_stage==2;"
"        blit_after_failure=blit_capture_metadata(blit_target_entry_num);"
"        blit_loop_addresses_unchanged=blit_before_set_loop.count==blit_after_failure.count&&blit_same_records(blit_before_set_loop.loop,blit_after_failure.loop,blit_before_set_loop.count);"
"        blit_loop_addresses_valid=blit_after_failure.loop_valid; blit_loop_addresses_reserved=blit_after_failure.loop_reserved;"
"        blit_backbuffers_unchanged=blit_same_backbuffer_metadata(&blit_before_set_loop,&blit_after_failure);"
"        blit_backbuffers_valid=blit_after_failure.backbuffer_valid; blit_backbuffers_reserved=blit_after_failure.backbuffer_reserved;"
"        blit_metadata_unchanged=blit_backbuffers_unchanged; blit_failure_active=false; blit_pending_parent_phase=true;"
"    }"
"}"
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
"        fprintf(blit_failure_csv,\"phase,setloop_entered,setloop_returned,setloop_success,setloop_bank_restored,second_allocator_entered,agi_result,entry_depth,nested_depth,agi_return_depth,parent_return_depth,bank_preserved,rom_preserved,i_preserved,load_view,set_view,draw_command,view_loaded,loop_unchanged,loop_zero,loop_valid,loop_reserved,backbuffers_unchanged,backbuffer_zero,backbuffer_valid,backbuffer_reserved,metadata_unchanged,retry_entered,retry_result,retry_set_loop_entries,retry_loop_metadata_consistent,retry_backbuffers_valid,retry_backbuffers_reserved,retry_metadata_committed,retry_bank_preserved,retry_rom_preserved,retry_i_preserved,retry_depth_restored\\n\"); fflush(blit_failure_csv);\n"
"        blit_failure_stage=0; view_load_test_begin(); return;\n"
"    }\n"
"    if(picture_test_stage==4 && shake_read(S_cwCurrentCode)==2 && !shake_read(S_cwCurrentCode+1)) {\n"
"        picture_restore_injection(); picture_check_restored(true);\n"
"        if(view_load_failure_command_entries!=1 || view_load_failure_caller_entries!=1 || view_load_failure_loader_entries!=1 || !view_load_loader_returned || view_load_loader_result || !view_load_failure_returned || !view_load_view_unchanged || !view_load_metadata_unchanged || !view_load_allocator_unchanged) { fprintf(stderr,\"Failed load.view did not leave cold view state unpublished: command/caller/loader=%u/%u/%u result=%u return=%u unchanged=%u/%u/%u \",view_load_failure_command_entries,view_load_failure_caller_entries,view_load_failure_loader_entries,view_load_loader_result,view_load_failure_returned,view_load_view_unchanged,view_load_metadata_unchanged,view_load_allocator_unchanged); exit(3); }\n"
"        picture_inject_blit_commands(); return;\n"
"    }\n"
"    if(picture_test_stage==5 && shake_read(S_cwCurrentCode)==7 && !shake_read(S_cwCurrentCode+1)) {\n"
"        picture_restore_injection(); picture_check_restored(true);\n"
"        if(blit_load_view_entries!=1 || blit_set_view_entries!=1 || blit_draw_entries!=1 || view_load_retry_caller_entries!=1 || view_load_retry_loader_entries!=1 || !view_load_retry_loader_returned || !view_load_retry_loader_result || !shake_banked_read(S__loadedViews+view_load_target*shake_read(S__sizeOfView),0x11)) { fprintf(stderr,\"Native load.view retry did not complete real load.view/set.view/draw: commands=%u/%u/%u callers=%u loaders=%u result=%u loaded=%u\\n\",blit_load_view_entries,blit_set_view_entries,blit_draw_entries,view_load_retry_caller_entries,view_load_retry_loader_entries,view_load_retry_loader_result,shake_banked_read(S__loadedViews+view_load_target*shake_read(S__sizeOfView),0x11)); exit(3); }\n"
"        blit_failure_stage=1; picture_test_stage=6; return;\n"
"    }\n"
"    if(picture_test_stage==6 && !view_load_retry_blit_done && clockticks6502>TRIGGER_CYCLE+50000000u) { fprintf(stderr,\"No real agiBlit occurred after load.view retry for view %u\\n\",view_load_target); exit(3); }\n"
"}\n"
"static void shake_before_step(void) {\n"
"    if(STARTUP_TEST_ROOM) { startup_before_step(); return; }\n"
"    if(LOADER_TEST_ENABLED) { loader_before_step(regs.pc,memory_get_ram_bank()); return; }\n"
"    if(STARTUP_TEXT_TEST_ENABLED) { startup_graphics_abi_check(regs.pc,memory_get_ram_bank()); startup_text_before_step(regs.pc,memory_get_ram_bank()); return; }\n"
"    static uint32_t next_progress=100000000u; if(clockticks6502>=next_progress) { fprintf(stderr,\"Native rendering progress: cycles=%u host_ms=%u command=%u picture_stage=%u pc=%04x bank=%u room=%u cursor=%u:%u handler=%u frames=%u depth=%u\\n\",clockticks6502,SDL_GetTicks(),shake_command_index,picture_test_stage,regs.pc,memory_get_ram_bank(),shake_banked_read(shake_var_address(0),0),shake_read(S_cwCurrentCode+1),shake_read(S_cwCurrentCode),shake_command_handler_seen,shake_frames_seen,shake_read(S__foregroundVeraDepth)); next_progress+=100000000u; }\n"
"    if(clockticks6502>=TRIGGER_CYCLE&&clockticks6502-TRIGGER_CYCLE>200000000u) { fprintf(stderr,\"Native rendering workload exceeded its 200M post-trigger cycle limit\\n\"); exit(3); }\n"
"    /* Match natural startup before saving interpreter pointers for injected workloads. */\n"
"    static uint32_t next_start_key=8000000u; if(!shake_view_warm_started&&clockticks6502>=next_start_key&&shake_banked_read(shake_var_address(0),0)!=1) { static char key[]=\"\\r\"; machine_paste(key,false); next_start_key=clockticks6502+8000000u; }\n"
"    if(text_case_active&&!waiting) text_case_track_source_helpers(regs.pc);\n"
"    shake_check_sprite_owner_wrapper_return();\n"
"    if(shake_sprite_owner_wrapper_active&&!shake_sprite_owner_wrapper_progress_logged&&clockticks6502-shake_sprite_owner_wrapper_start_cycle>500000u) { shake_sprite_owner_wrapper_progress_logged=true; fprintf(stderr,\"Sprite-owner wrapper progress: pc=%04x bank=%u codebank=%u SP=%04x E=%u depth=%u Cstack=%04x return=%04x/%04x target=%04x\\n\",regs.pc,memory_get_ram_bank(),regs.k,regs.sp,regs.e,shake_read(S__foregroundVeraDepth),combined_read16(S_c_sp,0),shake_sprite_owner_wrapper_return_pc,shake_sprite_owner_wrapper_return_sp,shake_sprite_owner_wrapper_is_end?S__testEndSpritePublicationWork:S__testBeginSpritePublicationWork); }\n"
"    if(!shake_csv) {\n"
"        shake_csv=fopen(SHAKE_CSV,\"w\"); shake_commands=fopen(SHAKE_COMMANDS,\"w\");\n"
"        if(!shake_csv || !shake_commands) { perror(\"profile output\"); exit(2); }\n"
"        fprintf(shake_csv,\"cycle,irq_cycles,custom_cycles,accept_delay,custom_delay,count,phase,channels,calls,psg_writes,window_writes,first_write,last_write,audible_writes,sprite_writes,input_chars,hstart,hstop,vstart,vstop,border,phase_after,frames_after\\n\");\n"
"        fprintf(shake_commands,\"opcode,operand,entry_cycle,exit_cycle,vsyncs,display_restored,code_window_restored,cursor_restored,window_writes,actual_command_entered,kbd_queue\\n\");\n"
"        shake_input_trace=fopen(SHAKE_INPUT_TRACE,\"w\"); if(!shake_input_trace) { perror(SHAKE_INPUT_TRACE); exit(2); } fprintf(shake_input_trace,\"cycle,pc,bank,command,queue_depth,input_line,input_dirty,input_char,var19\\n\");\n"
"    }\n"
"    if(regs.k || waiting) return;\n"
"    uint16_t pc=regs.pc; uint8_t bank=memory_get_ram_bank();\n"
"    if(bank==9 && pc==S_b9UpdateObjects) shake_real_b9_entries++;\n"
"    if(shake_view_warm_active&&!shake_view_warm_progress_logged&&clockticks6502-shake_view_warm_start_cycle>5000000u) { shake_view_warm_progress_logged=true; fprintf(stderr,\"Warm sprite progress: pc=%04x bank=%02x codebank=%u depth=%u Cstack=%04x commands=%u/%u/%u/%u/%u b9=%u snapshots=%u/%u owner=%u begin/end=%u/%u wrapper=%u return=%04x/%02x\\n\",pc,bank,regs.k,shake_read(S__foregroundVeraDepth),combined_read16(S_c_sp,0),shake_view_warm_load_entries,shake_view_warm_animate_entries,shake_view_warm_set_entries,shake_view_warm_position_entries,shake_view_warm_draw_entries,shake_view_warm_total_b9_entries,shake_view_warm_first_snapshot_saved,shake_view_warm_latest_snapshot_saved,shake_view_warm_outer_owner_active,shake_view_warm_outer_owner_begin_completed,shake_view_warm_outer_owner_end_completed,shake_sprite_owner_wrapper_active,shake_sprite_owner_wrapper_return_pc,shake_sprite_owner_wrapper_return_sp); }\n"
"    if(shake_view_warm_active&&bank==9&&pc==S_b9UpdateObjects&&!shake_view_warm_outer_owner_begin_scheduled&&shake_view_warm_load_entries==1&&shake_view_warm_animate_entries==1&&shake_view_warm_set_entries==1&&shake_view_warm_position_entries==1&&shake_view_warm_draw_entries==1) { shake_view_warm_outer_owner_begin_scheduled=true; shake_call_sprite_owner_wrapper(S__testBeginSpritePublicationWork,0,0,1); return; }\n"
"    if(pc==S__b6Interpret) combined_interpret_hits++;\n"
"    if(pc==S__executeLogic) combined_logic_hits++;\n"
"    if(pc==S_mainLoop) { combined_mainloop_hits++; if(!combined_first_mainloop_cycle) combined_first_mainloop_cycle=clockticks6502; combined_last_mainloop_cycle=clockticks6502; }\n"
"    if(pc==S__b5GetLogicEntry) combined_logic_entry_hits++;\n"
"    if(pc==S__b7PollKeyboard) combined_poll_hits++;\n"
"    if(shake_start_return_queued&&!shake_start_return_seen&&!shake_start_poll_active&&bank==7&&pc==S__b7PollKeyboard) { shake_start_poll_return_pc=shake_call_return(&shake_start_poll_return_sp); shake_start_poll_active=true; }\n"
"    if(shake_start_poll_active&&pc==shake_start_poll_return_pc&&(uint8_t)regs.sp==shake_start_poll_return_sp) { uint8_t key=shake_banked_read(shake_var_address(19),0),pending=debug_read6502(NDX,0,0); fprintf(shake_input_trace,\"%u,%04x,%u,start-return,0,%u,%u,%u,%02x,%04x\\n\",clockticks6502,pc,bank,pending,shake_read(S__inputLineDisplayed),shake_read(S__inputLineDirty),debug_read6502(S__b7CurrentInputStr,0,7),key); fflush(shake_input_trace); if(key==0x0d) { shake_start_return_seen=true; fprintf(stderr,\"KQ1 start Return was consumed by the production input poll (pending=%u)\\n\",pending); } shake_start_poll_active=false; }\n"
"    if(shake_keyboard_queued && bank==7 && pc==S__b7PollKeyboard) { fprintf(shake_input_trace,\"%u,%04x,%u,%u,%u,%u,%u,%02x,%04x\\n\",clockticks6502,pc,bank,shake_command_index,debug_read6502(NDX,0,0),shake_read(S__inputLineDisplayed),shake_read(S__inputLineDirty),debug_read6502(S__b7CurrentInputStr,0,7),shake_banked_read(shake_var_address(19),0)); fflush(shake_input_trace); }\n"
"    shake_check_sprite_batch_handler(pc,bank);\n"
"    if(COMBINED_IRQ_TEST_ENABLED) combined_before_step(pc,bank);\n"
"    if(shake_view_warm_active&&bank==9&&pc==S_b9UpdateObjects) { if(shake_view_warm_outer_owner_active) shake_view_warm_outer_owner_b9_entries++; shake_view_warm_total_b9_entries++; shake_view_warm_b9_entries++; shake_view_warm_b9_since_handler++; uint16_t pointer=combined_read16(S__bESpritesUpdatedBufferPointer,14); if(pointer!=S__bESpritesUpdatedBuffer) shake_view_warm_stale_entry_seen=true; if(shake_view_warm_call_count<256) { WarmSpriteProducerCall *call=&shake_view_warm_calls[shake_view_warm_call_count++]; call->entry_cycle=clockticks6502; call->entry_pointer=pointer; call->entry_depth=shake_read(S__foregroundVeraDepth); call->return_pc=shake_call_return(&call->return_sp); call->exit_cycle=0; call->exit_pointer=0; call->exit_depth=0; call->complete=0; shake_view_warm_active_call=shake_view_warm_call_count; } else { fprintf(stderr,\"Warm sprite producer call trace exceeded its bound\\n\"); exit(3); } }\n"
"    if(shake_view_warm_active_call&&pc==shake_view_warm_calls[shake_view_warm_active_call-1].return_pc&&(uint8_t)regs.sp==shake_view_warm_calls[shake_view_warm_active_call-1].return_sp) { WarmSpriteProducerCall *call=&shake_view_warm_calls[shake_view_warm_active_call-1]; call->exit_cycle=clockticks6502; call->exit_pointer=combined_read16(S__bESpritesUpdatedBufferPointer,14); call->exit_depth=shake_read(S__foregroundVeraDepth); call->complete=1; shake_record_warm_sprite_publication(call); shake_maybe_schedule_sprite_owner_release(call); shake_view_warm_active_call=0; }\n"
"    if(shake_view_warm_active&&bank==14&&pc==S_bEHandleSpriteUpdates) { shake_view_warm_sprite_handler_entries++; shake_view_warm_last_batch_b9_entries=shake_view_warm_b9_since_handler; shake_view_warm_b9_since_handler=0; shake_view_warm_last_batch_start=shake_view_warm_call_count>=shake_view_warm_last_batch_b9_entries?shake_view_warm_call_count-shake_view_warm_last_batch_b9_entries:0; }\n"
"    if(!shake_sprite_batch_saved&&shake_view_warm_active&&shake_view_warm_latest_snapshot_saved&&bank==14&&pc==S_bEHandleSpriteUpdates) shake_capture_sprite_batch();\n"
"    if(shake_sprite_batch_replay_active&&!shake_sprite_batch_handler_active&&bank==14&&pc==S_bEHandleSpriteUpdates) shake_begin_sprite_batch_replay();\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage && bank==2 && pc==S__b2Load_pic) picture_load_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage && bank==2 && pc==S__b2Draw_pic) picture_draw_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage==4 && bank==2 && pc==S__b2Load_view) view_load_failure_command_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage==5 && bank==2 && pc==S__b2Load_view) blit_load_view_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage==5 && bank==2 && pc==S__b2Set_view) blit_set_view_entries++;\n"
"    if(PICTURE_TEST_ENABLED && picture_test_stage==5 && bank==2 && pc==S__b2Draw) blit_draw_entries++;\n"
"    if(shake_view_warm_active&&bank==2) { if(pc==S__b2Load_view) shake_view_warm_load_entries++; if(pc==S__b2Animate_obj) shake_view_warm_animate_entries++; if(pc==S__b2Set_view) shake_view_warm_set_entries++; if(pc==S__b2Position) shake_view_warm_position_entries++; if(pc==S__b2Draw) shake_view_warm_draw_entries++; }\n"
"    if(foreground_irq_active && pc==foreground_irq_return_pc && (uint8_t)regs.sp==foreground_irq_return_sp) {\n"
"        uint32_t cycles=clockticks6502-foreground_irq_start_cycles; if(cycles>foreground_irq_max_cycles) foreground_irq_max_cycles=cycles; foreground_irq_count++;\n"
"        if(regs.a!=foreground_irq_a || regs.xl!=foreground_irq_x || regs.yl!=foreground_irq_y || (regs.status&0xcf)!=foreground_irq_status || memory_get_ram_bank()!=foreground_irq_ram_bank || memory_get_rom_bank()!=foreground_irq_rom_bank) { fprintf(stderr,\"Foreground IRQ changed CPU registers/flags/banks\\n\"); exit(3); }\n"
"        for(int i=0;i<4;i++) if(foreground_vera_state(i)!=foreground_irq_vera[i]) { fprintf(stderr,\"Foreground IRQ changed VERA address/control state\\n\"); exit(3); }\n"
"        if(video_get_dc_value(8)!=foreground_irq_fx[0]||video_get_dc_value(11)!=foreground_irq_fx[1]||video_get_fx_accum()!=foreground_irq_fx_accum) { fprintf(stderr,\"Foreground sound changed FX control or multiplier state\\n\"); exit(3); } for(int i=0;i<4;i++) if(video_get_dc_value(24+i)!=foreground_irq_fx[i+2]) { fprintf(stderr,\"Foreground sound changed FX cache bytes\\n\"); exit(3); }\n"
"        for(int i=0;i<0x52;i++) if((0x22+i<S__ZP_CURRENTLY_PLAYING_NOTE_1||0x22+i>=S__ZP_CURRENTLY_PLAYING_NOTE_1+8)&&shake_read(0x22+i)!=foreground_irq_zp[i]) { fprintf(stderr,\"Foreground IRQ changed zero-page scratch at $%02x\\n\",0x22+i); exit(3); }\n"
"        for(int i=0;i<0x57;i++) if(shake_read(0xa9+i)!=foreground_irq_zp[0x52+i]) { fprintf(stderr,\"Foreground IRQ changed zero-page scratch at $%02x\\n\",0xa9+i); exit(3); }\n"
"        for(int i=0;i<500;i++) if(shake_read(0x602+i)!=foreground_irq_work[i]) { fprintf(stderr,\"Foreground IRQ changed renderer work area at $%04x\\n\",0x602+i); exit(3); }\n"
"        uint8_t i2c_state[8]; shake_get_i2c_state(i2c_state); if(memcmp(i2c_state,foreground_irq_i2c,sizeof(i2c_state))) { fprintf(stderr,\"Foreground IRQ changed I2C protocol state\\n\"); exit(3); }\n"
"        foreground_irq_active=false;\n"
"    }\n"
"    if(FOREGROUND_TEST_ENABLED && !foreground_active) { if(bank==0x11 && pc==S__b11DrawPic) foreground_begin(1); else if(bank==9 && pc==S_b9UpdateObjects) foreground_begin(2); }\n"
"    if(foreground_active && pc==foreground_return_pc && (uint8_t)regs.sp==foreground_return_sp) foreground_finish();\n"
"    if(shake_view_warm_outer_owner_release_pending&&!shake_view_warm_outer_owner_end_scheduled&&pc==shake_view_warm_outer_owner_release_pc&&(uint8_t)regs.sp==shake_view_warm_outer_owner_release_sp) { shake_view_warm_outer_owner_release_pending=false; shake_view_warm_outer_owner_end_scheduled=true; shake_call_sprite_owner_wrapper(S__testEndSpritePublicationWork,1,1,0); return; }\n"
"    if(picture_draw_active) { if(pc==S__b8AsmFloodFillSections) picture_floodfill_count++; if(pc==S__b11SplitPriority) picture_split_count++; }\n"
"    if(PICTURE_TEST_ENABLED) view_load_failure_before_step(pc,bank);\n"
"    if(PICTURE_TEST_ENABLED) view_load_retry_before_step(pc,bank);\n"
"    if(PICTURE_TEST_ENABLED) blit_failure_before_step(pc,bank);\n"
"    if(PICTURE_TEST_ENABLED) { picture_test_before_step(pc); return; }\n"
"    if(shake_injected && shake_command_index==2 && !shake_keyboard_queued && clockticks6502-shake_command_start>200000) {\n"
"        static char key[]=\"A\"; machine_paste(key,false); shake_keyboard_queued=true;\n"
"    }\n"
"    if(!COMBINED_IRQ_TEST_ENABLED && pc==S_custom_irq_handler) {\n"
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
"    if(shake_injected && ((shake_command_index==0 && bank==3 && pc==S_b3Load_soundCCall) || (shake_command_index==1 && bank==3 && pc==S_b3Play_soundCCall) || (shake_command_index>=2 && bank==4 && pc==S_b4ShakeScreen))) shake_command_handler_seen=true;\n"
"    if(shake_injected && pc==S_mainLoop && shake_read(S_cwCurrentCode)==shake_command_lengths[shake_command_index] && !shake_read(S_cwCurrentCode+1)) shake_command_cursor_seen=true;\n"
"    if(pc!=S_mainLoop || shake_irq_active || clockticks6502<TRIGGER_CYCLE) return;\n"
"    if(!shake_view_warm_started&&(shake_banked_read(shake_var_address(0),0)!=1||shake_banked_read(shake_flag_address(5),0))) return;\n"
"    if(!shake_capture_started) { warp_mode=false; emu_write(5,2); shake_capture_started=true; shake_capture_start_frames=shake_frames_seen; return; }\n"
"    if(!shake_view_warm_started) { uint8_t loaded; uint16_t code,end; if(!shake_logic0_interpreter_ready(&loaded,&code,&end)) { static bool waiting_reported; if(!waiting_reported) { fprintf(stderr,\"Waiting for real LOGIC0 interpreter context before warm injection: loaded=%u code=%04x end=%04x\\n\",loaded,code,end); waiting_reported=true; } if(clockticks6502-TRIGGER_CYCLE>50000000u) { fprintf(stderr,\"Real LOGIC0 was not loaded into a valid banked interpreter context before the warm fixture timeout: loaded=%u code=%04x end=%04x\\n\",loaded,code,end); exit(3); } return; } shake_begin_view_warm(); return; }\n"
"    if(shake_view_warm_active) {\n"
"        if(!shake_view_warm_second_script_injected&&shake_view_warm_first_snapshot_saved) { if(shake_view_warm_load_entries!=1||shake_view_warm_animate_entries!=1||shake_view_warm_set_entries!=1||shake_view_warm_position_entries!=1||shake_view_warm_draw_entries!=1) { fprintf(stderr,\"Initial KQ1 warm-up did not execute its production command sequence exactly once before the second phase: load/animate/set/position/draw=%u/%u/%u/%u/%u\\n\",shake_view_warm_load_entries,shake_view_warm_animate_entries,shake_view_warm_set_entries,shake_view_warm_position_entries,shake_view_warm_draw_entries); exit(3); } static const uint8_t followup[]={37,1,12,11,35,1\n"
"#if COMBINED_IRQ_TEST_ENABLED\n"
"            ,20,0,156,120,98,0,99,0,240\n"
"#endif\n"
"        }; uint16_t code_base=(uint16_t)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8)),test_end=(uint16_t)(code_base+49); for(unsigned i=0;i<sizeof(followup);i++) shake_ram(S_codeWindow+i,followup[i]); shake_ram(S_codeWindow+sizeof(followup),0); shake_ram(S_cwCurrentCode,0); shake_ram(S_cwCurrentCode+1,0); shake_ram(S_endPos,(uint8_t)test_end); shake_ram(S_endPos+1,(uint8_t)(test_end>>8)); shake_ram(S_codeWindowInvalid,0); shake_view_warm_script_length=(uint8_t)sizeof(followup); shake_view_warm_second_script_injected=true; fprintf(stderr,\"Injected the second production position/draw as a fresh AGI command stream after the first pending snapshot\\n\"); return; }\n"
"        if(!shake_view_warm_script_complete&&shake_view_warm_second_script_injected&&shake_read(S_cwCurrentCode)==shake_view_warm_script_length&&!shake_read(S_cwCurrentCode+1)) { if(shake_view_warm_load_entries!=1||shake_view_warm_animate_entries!=1||shake_view_warm_set_entries!=1||shake_view_warm_position_entries!=2||shake_view_warm_draw_entries!=2) { fprintf(stderr,\"KQ1 warm-up did not execute the production command sequence twice: load/animate/set/position/draw=%u/%u/%u/%u/%u\\n\",shake_view_warm_load_entries,shake_view_warm_animate_entries,shake_view_warm_set_entries,shake_view_warm_position_entries,shake_view_warm_draw_entries); exit(3); } shake_view_warm_script_complete=true; }\n"
"        if(shake_view_warm_script_complete&&shake_sprite_batch_saved&&shake_sprite_batch_processed&&shake_view_warm_latest_snapshot_saved&&shake_view_warm_coordinates_changed&&shake_view_warm_second_batch_consumed) { shake_finish_view_warm(); return; }\n"
"        if(clockticks6502-shake_view_warm_start_cycle>50000000u) { fprintf(stderr,\"KQ1 real sprite producer warm-up timed out: script=%u injected=%u cursor=%u:%u commands=%u/%u/%u/%u/%u b9=%u snapshots=%u/%u changed=%u pending=%u captured/processed=%u/%u length=%u handlers=%u empty=%u pointer=%04x first=%02x%02x elapsed=%u\\n\",shake_view_warm_script_complete,shake_view_warm_second_script_injected,shake_read(S_cwCurrentCode+1),shake_read(S_cwCurrentCode),shake_view_warm_load_entries,shake_view_warm_animate_entries,shake_view_warm_set_entries,shake_view_warm_position_entries,shake_view_warm_draw_entries,shake_view_warm_b9_entries,shake_view_warm_first_snapshot_saved,shake_view_warm_latest_snapshot_saved,shake_view_warm_coordinates_changed,shake_view_warm_second_entry_pending,shake_sprite_batch_saved,shake_sprite_batch_processed,shake_sprite_batch_length,shake_sprite_batch_handler_entries,shake_sprite_batch_empty_entries,shake_sprite_batch_last_pointer,shake_sprite_batch_last_second,shake_sprite_batch_last_first,clockticks6502-shake_view_warm_start_cycle); exit(3); }\n"
"        return;\n"
"    }\n"
"    if(!shake_view_warm_finished||!shake_sprite_batch_saved||!shake_sprite_batch_processed||!shake_sprite_batch_length) { fprintf(stderr,\"KQ1 sprite batch was not produced and processed before foreground injection: finished=%u captured=%u processed=%u length=%u\\n\",shake_view_warm_finished,shake_sprite_batch_saved,shake_sprite_batch_processed,shake_sprite_batch_length); exit(3); }\n"
"    if(COMBINED_IRQ_TEST_ENABLED) return;\n"
"    if(shake_injected) {\n"
"        if(shake_frames_seen-shake_start_frames<16) return;\n"
"    } else if(shake_frames_seen-shake_capture_start_frames<16) return;\n"
"    if(shake_injected) {\n"
"        int length=shake_command_lengths[shake_command_index];\n"
"        if(!shake_command_cursor_seen || !shake_command_handler_seen) {\n"
"            fprintf(stderr,\"Injected command did not reach its production handler/cursor: command=%d opcode=%u operand=%u expected=%d actual=%u:%u handler=%u cursor=%u code=%04x end=%04x saved_code=%04x saved_end=%04x invalid=%u saved_invalid=%u frames=%u elapsed=%u command_entered=%u\\n\",\n"
"                shake_command_index,shake_commands_data[shake_command_index][0],shake_commands_data[shake_command_index][1],length,\n"
"                shake_read(S_cwCurrentCode+1),shake_read(S_cwCurrentCode),\n"
"                shake_command_handler_seen,\n"
"                shake_command_cursor_seen,\n"
"                (unsigned)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8)),\n"
"                (unsigned)(shake_read(S_endPos)|((uint16_t)shake_read(S_endPos+1)<<8)),\n"
"                (unsigned)(shake_saved_pointer[0]|((uint16_t)shake_saved_pointer[1]<<8)),\n"
"                (unsigned)(shake_saved_end[0]|((uint16_t)shake_saved_end[1]<<8)),\n"
"                shake_read(S_codeWindowInvalid),shake_saved_code_window_invalid,\n"
"                shake_frames_seen,shake_frames_seen-shake_start_frames,shake_command_entered);\n"
"            exit(3);\n"
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
"        for(int i=0;i<2;i++) shake_ram(S_endPos+i,shake_saved_end[i]);\n"
"        shake_ram(S_codeWindowInvalid,shake_saved_code_window_invalid);\n"
"        for(int i=0;i<50;i++) if(shake_read(S_codeWindow+i)!=shake_saved_window[i]) exit(3);\n"
"        for(int i=0;i<2;i++) if(shake_read(S_cwCurrentCode+i)!=shake_saved_cursor[i] || shake_read(0xa9+i)!=shake_saved_pointer[i] || shake_read(S_endPos+i)!=shake_saved_end[i]) exit(3);\n"
"        if(shake_read(S_codeWindowInvalid)!=shake_saved_code_window_invalid) exit(3);\n"
"        fprintf(shake_commands,\"%u,%u,%u,%u,%u,1,1,1,%u,%u,%u\\n\",shake_commands_data[shake_command_index][0],\n"
"            shake_commands_data[shake_command_index][1],shake_command_start,clockticks6502,shake_frames_seen-shake_start_frames,shake_total_writes-shake_command_writes,shake_command_handler_seen,debug_read6502(NDX,0,0));\n"
"        fflush(shake_commands);\n"
"        shake_injected=false; shake_command_index++;\n"
"        if(shake_command_index==5) { shake_last_command_frames=shake_frames_seen; shake_capture_end=shake_frames_seen+120; return; }\n"
"    }\n"
"    if(shake_command_index==5) return;\n"
"    fprintf(stderr,\"Inject command=%d opcode=%u operand=%u old-code=%04x old-end=%04x invalid=%u current-code=%04x current-end=%04x\\n\",shake_command_index,shake_commands_data[shake_command_index][0],shake_commands_data[shake_command_index][1],(unsigned)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8)),(unsigned)(shake_read(S_endPos)|((uint16_t)shake_read(S_endPos+1)<<8)),shake_read(S_codeWindowInvalid),(unsigned)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8)),(unsigned)(shake_read(S_endPos)|((uint16_t)shake_read(S_endPos+1)<<8)));\n"
"    for(int i=0;i<50;i++) shake_saved_window[i]=shake_read(S_codeWindow+i);\n"
"    for(int i=0;i<2;i++) {\n"
"        shake_saved_cursor[i]=shake_read(S_cwCurrentCode+i);\n"
"        shake_saved_pointer[i]=shake_read(0xa9+i);\n"
"        shake_ram(S_cwCurrentCode+i,0);\n"
"    }\n"
"    for(int i=0;i<2;i++) shake_saved_end[i]=shake_read(S_endPos+i);\n"
"    shake_saved_code_window_invalid=shake_read(S_codeWindowInvalid);\n"
"    shake_ram(S_codeWindowInvalid,0);\n"
"    uint16_t code_base=(uint16_t)(shake_read(0xa9)|((uint16_t)shake_read(0xaa)<<8));\n"
"    uint16_t test_end=code_base+49;\n"
"    shake_ram(S_endPos,(uint8_t)test_end); shake_ram(S_endPos+1,(uint8_t)(test_end>>8));\n"
"    for(int i=0;i<shake_command_lengths[shake_command_index];i++)\n"
"        shake_ram(S_codeWindow+i,shake_commands_data[shake_command_index][i]);\n"
"    shake_ram(S_codeWindow+shake_command_lengths[shake_command_index],0); // Return from the injected script after this command.\n"
"    if(shake_commands_data[shake_command_index][0]==110) {\n"
"        // Exercise real menu drawing for count1; real score/status redraw for count2.\n"
"        shake_ram(S__menuShown,shake_command_index==2);\n"
"        shake_ram(S__menuDirty,1);\n"
"        shake_ram(0x403,shake_read(0x403)+1);\n"
"        shake_ram(S__mainLoopCounter,shake_read(S__mainLoopCounter)+1);\n"
"        // Replay the complete game-produced batch, including its terminator, through the real IRQ.\n"
"        unsigned records=(shake_sprite_batch_length-SHAKE_SPRITE_TERMINATOR_BYTES)/SHAKE_SPRITE_UPDATE_RECORD_BYTES; if(!shake_sprite_batch_saved||!shake_sprite_batch_processed||!records) { fprintf(stderr,\"No processed nonempty game-produced sprite batch is available for replay (captured=%u processed=%u records=%u)\\n\",shake_sprite_batch_saved,shake_sprite_batch_processed,records); exit(3); } // Stage after the last foreground producer, at the real handler publication boundary.\n"
"        if(shake_command_index==2) { shake_sprite_batch_replayed=false; shake_sprite_batch_replay_requested=true; shake_sprite_batch_replay_active=true; }\n"
"        if(shake_command_index==4) {\n"
"            for(int i=0;i<40;i++) shake_saved_input[i]=debug_read6502(S__b7CurrentInputStr+i,0,7);\n"
"            const char input[]=\"LOOK\";\n"
"            for(int i=0;i<5;i++) shake_banked_ram(S__b7CurrentInputStr+i,7,input[i]);\n"
"            shake_ram(S__inputLineDirty,1);\n"
"            shake_input_replaced=true;\n"
"        }\n"
"    }\n"
"    shake_get_display(shake_saved_display); shake_command_writes=shake_total_writes; shake_command_entered=shake_command_handler_seen=false;\n"
"    shake_command_start=clockticks6502; shake_start_frames=shake_frames_seen; shake_command_cursor_seen=false; shake_injected=true;\n"
"}\n"
"static void shake_after_step(uint8_t opcode) {\n"
"    if(STARTUP_TEST_ROOM) return;\n"
"    if(LOADER_TEST_ENABLED) return;\n"
"    if(STARTUP_TEXT_TEST_ENABLED) return;\n"
"    if(COMBINED_IRQ_TEST_ENABLED) { combined_after_step(opcode); return; }\n"
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
"        if(shake_commands_data[shake_command_index][0]==110&&shake_sprite_batch_replay_requested&&!shake_sprite_batch_replayed) { fprintf(stderr,\"Replayed game sprite batch did not return through the production IRQ (saved=%u records=%u)\\n\",shake_sprite_batch_saved,(shake_sprite_batch_length-SHAKE_SPRITE_TERMINATOR_BYTES)/SHAKE_SPRITE_UPDATE_RECORD_BYTES); exit(3); }\n"
"        uint8_t display[5]; shake_get_display(display);\n"
"        fprintf(shake_csv,\"%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\\n\",clockticks6502,\n"
"            clockticks6502-shake_accept,clockticks6502-shake_custom,\n"
"            shake_accept-shake_entry_vsync,shake_custom-shake_entry_vsync,\n"
"            shake_entry_count,shake_entry_phase,shake_entry_channels,shake_calls,\n"
"            shake_psg_writes,shake_window_writes,shake_first_write,shake_last_write,shake_audible_writes,shake_sprite_writes,shake_input_chars,\n"
"            display[0],display[1],display[2],display[3],display[4],shake_read(S_shakePhases),shake_read(S_shakeFrames));\n"
"        fflush(shake_csv); shake_irq_active=false;\n"
"    }\n"
"    if(shake_keyboard_queued && !shake_keyboard_processed && debug_read6502(S__b7CurrentInputStr,0,7)=='a') {\n"
"        shake_keyboard_processed=true; shake_key_processed_frame=shake_frames_seen;\n"
"    }\n"
"    if(shake_capture_end && (shake_frames_seen>=shake_capture_end ||\n"
"       (shake_keyboard_processed && shake_frames_seen-shake_last_command_frames>=16 && shake_frames_seen-shake_key_processed_frame>=2))) {\n"
"        if(!shake_start_return_seen) { fprintf(stderr,\"Warm-up Return did not reach the production keyboard poll\\n\"); exit(3); }\n"
"        fprintf(stderr,\"Keyboard processed after resume: %u\\n\",shake_keyboard_processed);\n"
"        shake_banked_ram(shake_flag_address(9),0,shake_saved_flag9);\n"
"        emu_write(5,0); raise(SIGINT); shake_capture_end=0;\n"
"    }\n"
"}\n"
;
/* Observe natural game execution; Return and Right use the emulator keyboard. */
static const char startup_hook[] =
"/* KQ3 view 0 uses transparent colour 14 in every walking loop. */\n"
"static unsigned startup_ego_palette_checks,startup_ego_palette_room,startup_ego_palette_loops;\n"
"static void startup_check_ego_palette(unsigned room,uint16_t x,uint16_t y) {\n"
"    if(shake_banked_read(S__viewtab+shake_read(S__offsetOfCurrentView),9)!=0) return;\n"
"    unsigned height=shake_banked_read(S__viewtab+shake_read(S__offsetOfYSize),9);\n"
"    for(unsigned slot=0;slot<128;slot++) { unsigned a=0x1fc00+slot*8; if(!(video_space_read(a+6)&12)) continue; unsigned sx=video_space_read(a+2)|((video_space_read(a+3)&3)<<8),sy=video_space_read(a+4)|((video_space_read(a+5)&3)<<8); if((sx!=x*2&&sx+2!=x*2)||sy+height!=y+37) continue; unsigned palette=video_space_read(a+7)&15;\n"
"        for(unsigned c=1;c<16;c++) { unsigned base=0x1fa00+(c==14?0:c*2),actual=0x1fa00+palette*32+c*2; unsigned expected=video_space_read(base)|((video_space_read(base+1)&15)<<8),got=video_space_read(actual)|((video_space_read(actual+1)&15)<<8); if(got!=expected) { fprintf(stderr,\"KQ3 ego colour changed: room=%u loop=%u sprite=%u palette=%u colour=%u RGB=%03x expected=%03x\\n\",room,shake_banked_read(S__viewtab+shake_read(S__offsetOfCurrentLoop),9),slot,palette,c,got,expected); exit(3); } } startup_ego_palette_checks++; startup_ego_palette_room=room; if(room==34) startup_ego_palette_loops|=1u<<shake_banked_read(S__viewtab+shake_read(S__offsetOfCurrentLoop),9); return;\n"
"    }\n"
"}\n"
"static bool startup_story_chickens_visible(void) {\n"
"    for(unsigned object=1;object<=2;object++) {\n"
"        uint16_t tab=S__viewtab+object*shake_read(S__sizeOfViewTab);\n"
"        unsigned view=shake_banked_read(tab+shake_read(S__offsetOfCurrentView),9);\n"
"        if(view!=205&&view!=206) return false;\n"
"        int x=combined_read16(tab+shake_read(S__offsetOfXPos),9),y=combined_read16(tab+shake_read(S__offsetOfYPos),9);\n"
"        int height=shake_banked_read(tab+shake_read(S__offsetOfYSize),9); bool found=false;\n"
"        for(unsigned slot=0;slot<128;slot++) {\n"
"            unsigned a=0x1fc00+slot*8; if(!(video_space_read(a+6)&12)) continue;\n"
"            int sx=video_space_read(a+2)|((video_space_read(a+3)&3)<<8),sy=video_space_read(a+4)|((video_space_read(a+5)&3)<<8);\n"
"            if(abs(sx-x*2)<=2&&abs(sy+height-y-37)<=1) { found=true; break; }\n"
"        }\n"
"        if(!found) return false;\n"
"    }\n"
"    return true;\n"
"}\n"
"bool cx16agiOpeningReady;\n"
"static void startup_before_step(void) {\n"
"    if(WATCH_OPENING_ENABLED&&cx16agiOpeningReady) return;\n"
"    static unsigned dependency_walks;\n"
"    static unsigned sound_history_frame; static uint8_t sound_history_active[8],sound_history_owner[8],sound_history_bank[8],sound_owner_bank; static uint16_t sound_history_pc[8],sound_owner_pc;\n"
"    static unsigned pictures,room_pictures,polls,ticks,updates,room_ticks,room_polls,last_room=256;\n"
"    static unsigned colour_phase,colour_phase_tick; static unsigned kq3_intro_pictures,kq3_intro_stages,kq3_story_first_frame; static bool kq3_story_shown,kq3_chickens_verified; static bool kq3_intro_shown,showing; static uint16_t show_return; static uint8_t show_sp;\n"
"    static unsigned intro_rows,intro_first_frame,intro_last_frame,intro_sound_calls,intro_last_sound_frame,intro_max_sound_gap,intro_display_waits;\n"
"    static uint32_t intro_first_cycle,intro_last_cycle,intro_call_cycle,intro_display_cycles;\n"
"    static uint32_t intro_host_start; if(!intro_host_start) intro_host_start=SDL_GetTicks()+1;\n"
"    static bool moving,intro_display_active; static uint16_t start_x,start_y,intro_display_return; static uint8_t intro_display_sp;\n"
"    static uint32_t next_key=8000000u; static uint16_t picture_return; static uint8_t picture_sp; static bool drawing;\n"
"    if(clockticks6502>(STARTUP_TEST_ROOM==7?2000000000u:800000000u)) { fprintf(stderr,\"Startup timeout: expected room=%u actual=%u pictures=%u polls=%u ticks=%u updates=%u pc=%04x bank=%u\\n\",STARTUP_TEST_ROOM,last_room,pictures,polls,ticks,updates,regs.pc,memory_get_ram_bank()); exit(3); }\n"
"    if(INTRO_TEST_ENABLED&&intro_display_active&&waiting) intro_display_waits++;\n"
"    if(regs.k||waiting) return;\n"
"    uint16_t pc=regs.pc; uint8_t bank=memory_get_ram_bank();\n"
"    if(bank==4&&pc==S__b4LoadUnloadResources) dependency_walks++;\n"
"    if(INTRO_TEST_ENABLED&&bank==3&&pc==S__b3Display_v) { if(!intro_rows) { intro_first_frame=shake_frames_seen; intro_first_cycle=clockticks6502; } intro_rows++; intro_display_active=true; intro_call_cycle=clockticks6502; intro_display_return=shake_call_return(&intro_display_sp); }\n"
"    if(intro_display_active&&pc==intro_display_return&&(uint8_t)regs.sp==intro_display_sp) { intro_last_frame=shake_frames_seen; intro_last_cycle=clockticks6502; intro_display_cycles+=clockticks6502-intro_call_cycle; intro_display_active=false; }\n"
"    if(INTRO_TEST_ENABLED&&pc==S__b6EnterForegroundVera&&!shake_read(S__foregroundVeraDepth)) { uint8_t sp; sound_owner_pc=shake_call_return(&sp); sound_owner_bank=bank; }\n"
"    if(INTRO_TEST_ENABLED&&sound_history_frame!=shake_frames_seen) { sound_history_frame=shake_frames_seen; sound_history_active[shake_frames_seen%8]=shake_banked_read(S__bBChannelsPlaying,11); sound_history_owner[shake_frames_seen%8]=shake_read(S__foregroundVeraDepth); sound_history_pc[shake_frames_seen%8]=sound_owner_pc; sound_history_bank[shake_frames_seen%8]=sound_owner_bank; }\n"
"    if(INTRO_TEST_ENABLED&&bank==11&&pc==S_bBSoundHandler&&shake_banked_read(S__bBChannelsPlaying,11)) { unsigned gap=shake_frames_seen-intro_last_sound_frame; if(intro_sound_calls&&gap>intro_max_sound_gap) { intro_max_sound_gap=gap; if(gap>1) fprintf(stderr,\"Intro cadence gap=%u frame=%u prior_active=%u prior_owner=%u current_owner=%u caller=%02x:%04x\\n\",gap,shake_frames_seen,sound_history_active[(shake_frames_seen-1)%8],sound_history_owner[(shake_frames_seen-1)%8],shake_read(S__foregroundVeraDepth),sound_history_bank[(shake_frames_seen-1)%8],sound_history_pc[(shake_frames_seen-1)%8]); } intro_last_sound_frame=shake_frames_seen; intro_sound_calls++; }\n"
"    if(bank==17&&pc==S__b11DrawPic) { drawing=true; picture_return=shake_call_return(&picture_sp); }\n"
"    if(drawing&&pc==picture_return&&(uint8_t)regs.sp==picture_sp) { pictures++; unsigned room=shake_banked_read(shake_var_address(0),0); if(room==STARTUP_TEST_ROOM) room_pictures++; if(STARTUP_TEST_ROOM==7&&room==45) { kq3_intro_pictures++; unsigned stage=shake_banked_read(shake_var_address(226),0); if(stage==0||stage==2||stage==3) kq3_intro_stages|=1u<<stage; fprintf(stderr,\"KQ3 natural intro picture: stage=%u mask=%x frame=%u\\n\",stage,kq3_intro_stages,shake_frames_seen); } drawing=false; }\n"
"    if(STARTUP_TEST_ROOM==7&&bank==2&&pc==S__b2Show_pic&&(shake_banked_read(shake_var_address(0),0)==45||shake_banked_read(shake_var_address(0),0)==46)) { showing=true; show_return=shake_call_return(&show_sp); }\n"
"    if(showing&&pc==show_return&&(uint8_t)regs.sp==show_sp) { if(shake_banked_read(shake_var_address(0),0)==46&&!kq3_story_shown) { if(kq3_intro_stages!=13) { fprintf(stderr,\"KQ3 opening skipped title/credit pictures: mask=%x\\n\",kq3_intro_stages); exit(3); } kq3_story_shown=true; fprintf(stderr,\"KQ3 story visible after natural titles: frame=%u; withholding input for 120 more frames\\n\",shake_frames_seen); } if(!kq3_intro_shown) { fprintf(stderr,\"KQ3 intro visible: room=45 frame=%u; withholding input through the title and story opening\\n\",shake_frames_seen); } kq3_intro_shown=true; showing=false; }\n"
"    if(bank==7&&pc==S__b7PollKeyboard) polls++;\n"
"    if(bank==9&&pc==S_b9UpdateObjects) updates++;\n"
"    if(bank==6&&pc==S__b6Interpret) {\n"
"        ticks++; unsigned room=shake_banked_read(shake_var_address(0),0);\n"
"        if(STARTUP_TEST_ROOM==7&&room==46&&kq3_story_shown&&!kq3_chickens_verified&&startup_story_chickens_visible()) { kq3_chickens_verified=true; kq3_story_first_frame=shake_frames_seen; fprintf(stderr,\"KQ3 both chicken sprites are published: frame=%u\\n\",shake_frames_seen); }\n"
"        if(STARTUP_TEST_ROOM==7&&room==7&&(!kq3_intro_pictures||!kq3_intro_shown||!kq3_chickens_verified)) { fprintf(stderr,\"KQ3 skipped its intro: gameplay room 7 reached before the title/story opening completed; v70=%u restart_flag=%u\\n\",shake_banked_read(shake_var_address(70),0),shake_banked_read(shake_pointer_value(S__flag)+6,0)); exit(3); }\n"
"        if(STARTUP_TEST_ROOM==7&&room==7&&shake_frames_seen-kq3_story_first_frame<120) { fprintf(stderr,\"KQ3 intro dismissed without input after %u frames\\n\",shake_frames_seen-kq3_story_first_frame); exit(3); }\n"
"        if(INTRO_TEST_ENABLED&&STARTUP_TEST_ROOM==1) {\n"
"            if(intro_rows>=13) { unsigned span=intro_last_frame-intro_first_frame; fprintf(stderr,\"Natural intro credit batch: rows=%u visible_update_frames=%u active_sound_calls=%u max_sound_gap_frames=%u emulated_cycles=%u host_ms=%u batch_cycles=%u display_cycles=%u waits=%u\\n\",intro_rows,span,intro_sound_calls,intro_max_sound_gap,clockticks6502,SDL_GetTicks()+1-intro_host_start,intro_last_cycle-intro_first_cycle,intro_display_cycles,intro_display_waits); if(span>3||intro_last_cycle-intro_first_cycle>403200u||intro_display_waits) { fprintf(stderr,\"Intro credit batch exceeded its three-frame CPU budget or waited for display IRQs\\n\"); exit(3); } if(!intro_sound_calls||intro_max_sound_gap>1) { fprintf(stderr,\"Intro sound updates missed active VSYNC frames\\n\"); exit(3); } fprintf(stderr,\"PASS natural intro presentation\\n\"); exit(0); }\n"
"            intro_rows=0; intro_display_waits=0; intro_display_cycles=0;\n"
"        }\n"
"        if(room!=last_room) { fprintf(stderr,\"Natural startup: room=%u cycle=%u pictures=%u ticks=%u polls=%u v70=%u restart_flag=%u\\n\",room,clockticks6502,pictures,ticks,polls,shake_banked_read(shake_var_address(70),0),shake_banked_read(shake_pointer_value(S__flag)+6,0)); last_room=room; room_ticks=ticks; room_polls=polls; }\n"
"        unsigned sprites=0; for(unsigned i=0;i<128;i++) if(video_space_read(0x1fc00+i*8+6)&12) sprites++;\n"
"        uint16_t x=combined_read16(S__viewtab+shake_read(S__offsetOfXPos),9),y=combined_read16(S__viewtab+shake_read(S__offsetOfYPos),9);\n"
"        if(STARTUP_TEST_ROOM==7&&room!=45&&ticks-room_ticks>=3) startup_check_ego_palette(room,x,y);\n"
"        if(STARTUP_TEST_ROOM==7&&colour_phase==1&&room==34) { static char stop[]=\"\\x11\\x11\"; unsigned direction=shake_banked_read(shake_var_address(6),0); if(direction==5) machine_paste(stop,false); colour_phase=2; fprintf(stderr,\"Courtyard stop at %u,%u direction=%u\\n\",x,y,direction); colour_phase_tick=ticks; next_key=clockticks6502+8000000u; }\n"
"        if(STARTUP_TEST_ROOM==7&&room==34&&colour_phase>=2&&colour_phase<=6&&ticks-colour_phase_tick>=(colour_phase==2?10u:3u)&&startup_ego_palette_room==34) { static char open[]=\"\\x11OPEN DOOR\\r\"; if(colour_phase<6) { static char turns[][5]={\"\\\\X91\",\"\\\\X9D\",\"\\\\X1D\",\"\\\\X11\"}; machine_paste(turns[colour_phase-2],false); } else machine_paste(open,false); colour_phase++; fprintf(stderr,\"Courtyard phase=%u position=%u,%u loops=%x input=%u\\n\",colour_phase,x,y,startup_ego_palette_loops,shake_read(S__inputLineDisplayed)); colour_phase_tick=ticks; next_key=clockticks6502+8000000u; }\n"
"        if(STARTUP_TEST_ROOM==7&&colour_phase==7&&ticks%50==0) fprintf(stderr,\"Door waiting: position=%u,%u direction=%u unknown=%u input=%u loopmask=%x\\n\",x,y,shake_banked_read(shake_var_address(6),0),shake_banked_read(shake_var_address(9),0),shake_read(S__inputLineDisplayed),startup_ego_palette_loops);\n"
"        if(STARTUP_TEST_ROOM==7&&colour_phase==7&&room==7&&ticks-room_ticks>=10) { static char down[]=\"\\x11\"; machine_paste(down,false); colour_phase=8; next_key=clockticks6502+8000000u; }\n"
"        if(STARTUP_TEST_ROOM==7&&colour_phase==8&&room==34&&ticks-room_ticks>=10&&startup_ego_palette_room==34) { if(startup_ego_palette_loops!=15) { fprintf(stderr,\"Courtyard walking loops missing: mask=%x\\n\",startup_ego_palette_loops); exit(3); } fprintf(stderr,\"PASS natural game startup and courtyard round trip colours: checks=%u\\n\",startup_ego_palette_checks); exit(0); }\n"
"        if(room==STARTUP_TEST_ROOM&&ticks-room_ticks>=10&&polls-room_polls>=10&&room_pictures&&updates&&sprites) {\n"
"            if(!shake_banked_read(S__b4IsInited,4)||dependency_walks<6) { fprintf(stderr,\"Game skipped validated dependency metadata: initialized=%u walks=%u\\n\",shake_banked_read(S__b4IsInited,4),dependency_walks); exit(3); }\n"
"            if(!moving) { static char right[]=\"\\x1d\"; start_x=x; start_y=y; moving=true; machine_paste(right,false); fprintf(stderr,\"Startup movement: right from %u,%u\\n\",x,y); }\n"
"            else if((x!=start_x||y!=start_y)&&shake_banked_read(shake_var_address(6),0)==3) { fprintf(stderr,\"PASS natural game startup: room=%u ticks=%u polls=%u pictures=%u updates=%u visible_sprites=%u moved=%u,%u->%u,%u\\n\",room,ticks,polls,pictures,updates,sprites,start_x,start_y,x,y); if(STARTUP_TEST_ROOM!=7) exit(0); if(!colour_phase) { static char down[]=\"\\x11\"; machine_paste(down,false); colour_phase=1; next_key=clockticks6502+8000000u; } }\n"
"        }\n"
"    }\n"
"    if(INTRO_TEST_ENABLED&&STARTUP_TEST_ROOM==7&&kq3_chickens_verified&&shake_frames_seen-kq3_story_first_frame>=120) { fprintf(stderr,\"PASS natural intro presentation: KQ3 titles/credits mask=%x and story opening remained visible without input\\n\",kq3_intro_stages); if(WATCH_OPENING_ENABLED) { cx16agiOpeningReady=true; fprintf(stderr,\"KQ3 chickens/story scene verified; normal keyboard input is enabled\\n\"); return; } exit(0); }\n"
"    if(!INTRO_TEST_ENABLED&&(STARTUP_TEST_ROOM!=7||(kq3_chickens_verified&&shake_frames_seen-kq3_story_first_frame>=120))&&clockticks6502>=next_key&&(last_room!=STARTUP_TEST_ROOM||colour_phase)) { static char key[]=\"\\r\"; machine_paste(key,false); next_key=clockticks6502+8000000u; }\n"
"}\n";

static const char *names[] = {"custom_irq_handler","mainLoop","codeWindow","cwCurrentCode","endPos","codeWindowInvalid","b4ShakeScreen","b3Load_soundCCall","b3Play_soundCCall","b5Set_menuCCall","b2Load_logicsCCall","b3DisplayCCall","_b6LoadLogicFile","_b4IsInited","_b4LoadUnloadResources","_currentLog","_logicFiles","_logics","shakeCount","shakePhases","shakeFrames","_menuDirty","_menuShown","_menuAllowed","_numOfMenus","_charSetInited","_mainLoopCounter","_bBChannelsPlaying","_bBLoadedSoundsPointer","_bBEndSoundFlag","bBSoundHandler","b7HandleInputLine","bADisplayMenu","b10DisplayStatusBar","bEHandleSpriteUpdates","b10PrintStatusBarString","handleScreenShake","handleDisplayTextSlice","@handleSound","_inputLineDisplayed","_inputLineDirty","_b7CurrentInputStr","_bESpritesUpdatedBuffer","_bESpritesUpdatedBufferPointer","_b11DrawPic","_b2Load_pic","_b2Draw_pic","_b2Show_pic","_b3Display","_b3Display_v","_b2Load_view","_b2Animate_obj","_b2Set_view","_b2Position","_b2Draw","b9UpdateObjects","_b6LoadAGIFile","_b9LoadViewFile","_b7PollKeyboard","_viewdir","_numViews","_agiBlit","_bESetLoop","_bEAllocateSpriteMemory","_bDSpriteAllocTable","_viewtab","_viewTableMetadata","_loadedViews","_offsetOfCurrentView","_offsetOfCurrentLoop","_offsetOfCurrentCel","_offsetOfEntryNum","_offsetOfFlags","_offsetOfXPos","_offsetOfYPos","_offsetOfYSize","_offsetOfPrevX","_offsetOfPrevY","_offsetOfStopped","_offsetOfBackBuffers","_offsetOfViewMetadataBank","_offsetOfIsOnBackBuffer","_offsetOfloopsVeraAddressesPointers","_offsetOfLoops","_offsetOfLoopsBank","_offsetOfAllocationWidth","_offsetOfAllocationHeight","_offsetOfMaxVeraSlots","_sizeOfLoop","_sizeOfViewTab","_sizeOfView","findFreeVRamLowByteLoop","findFirstFreeVRamBlock_optimisticSkip","_vSyncCounter","_vSyncCounterHigh","_foregroundVeraDepth","_b6EnterForegroundVera","_ZP_CURRENTLY_PLAYING_NOTE_1","sendIrqCommand","irqWorkKind","irqCompletionGeneration","lastMainLoopCounter","_currentTextBuffer","_displayTextAddressToCopyTo","_displayTextAddressToCopyToHigh","_b3PaletteRows","_b8AsmFloodFillSections","_b11SplitPriority","_var","_flag","_b6Interpret","_executeLogic","_b5GetLogicEntry","_testRunCombinedDisplayText","_testMultiplyBoundsResults","_testRunBankAliasDisplay","_testRunBankBoundaryDisplay","_testRunFullLayerClear","_testBankAliasSource","_testBankAliasBank","_b3TextBuffers","_strLenBankedBounded","_memCpyBankedBetween","_memCpyBanked","_testCombinedSoundEndFlag","_testBeginSpritePublicationWork","_testEndSpritePublicationWork","c_sp","_b3PaletteNumber"};
#define NAME_COUNT (sizeof(names)/sizeof(names[0]))
static unsigned labels[NAME_COUNT]; static bool found[NAME_COUNT];
static const char *allocator_names[]={"_bEAllocateSpriteMemoryBulk","_bEBulkAllocatedAddresses","bDFindFreeVramBlock","_bDDeleteAllocation","bDDeleteAllocationAsmCall","VIEW_POS_ENTRY_NUM","sreg","BLOCKS_TO_FIND"};
#define ALLOCATOR_NAME_COUNT (sizeof(allocator_names)/sizeof(allocator_names[0]))
static unsigned allocator_labels[ALLOCATOR_NAME_COUNT]; static bool allocator_found[ALLOCATOR_NAME_COUNT];
static const char *loader_names[]={"_allocationArray","_numWords","_numSynonyms","_wordBank","_wordsData","_wordsDataBank","_wordPointers","_wordsPointersBank","_synonymsList","_synonymsListBank","_wordsTextStart","_b12LoadWords","_b12OpenWords","_b10BankedAlloc","_b6Initialise","_bDLoadObjectFile","_b7InitEvents","_b6InitInterpreter","_b6InitIrq","VIEW_POS_LOCAL_VIEW_TAB","_b0FReadWordsPass","_cbm_read","_b6Cx16_fseek","_exit"};
#define LOADER_NAME_COUNT (sizeof(loader_names)/sizeof(loader_names[0]))
static unsigned loader_labels[LOADER_NAME_COUNT]; static bool loader_found[LOADER_NAME_COUNT];
static const char *loader_search_names[]={"_testRunWordsSearch","_testWordsQuery","_testWordsResult"};
#define LOADER_SEARCH_NAME_COUNT (sizeof(loader_search_names)/sizeof(loader_search_names[0]))
static unsigned loader_search_labels[LOADER_SEARCH_NAME_COUNT]; static bool loader_search_found[LOADER_SEARCH_NAME_COUNT];
static const char *startup_names[]={"_b6InitGraphics"};
#define STARTUP_NAME_COUNT (sizeof(startup_names)/sizeof(startup_names[0]))
static unsigned startup_labels[STARTUP_NAME_COUNT]; static bool startup_found[STARTUP_NAME_COUNT];
static const char *fixture_names[]={"_testReserveSpriteFixtureBlocks","_testPrepareSpriteFixturePartialRun","_testReleaseSpriteFixtureBlocks","_testSpriteFixtureWidth","_testSpriteFixtureHeight","_testSpriteFixtureError","_testSpriteFixtureGapBlocks","_testSpriteFixtureAllocationCount","_testSpriteFixtureReleasedCount","_testSpriteFixtureGapAddress"};
#define FIXTURE_NAME_COUNT (sizeof(fixture_names)/sizeof(fixture_names[0]))
static unsigned fixture_labels[FIXTURE_NAME_COUNT]; static bool fixture_found[FIXTURE_NAME_COUNT];
static void load_symbols(const char *path) {
    if(!exists(path)) return;
    char *data=read_file(path,NULL),*save;
    for(char *line=strtok_r(data,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
        char name[512];unsigned address;
        if(sscanf(line,"al %x %511s",&address,name)==2) {
            char *n=name;while(*n=='.')n++;
            for(size_t i=0;i<NAME_COUNT;i++)if(!strcmp(n,names[i])) { labels[i]=address;found[i]=true; }
            for(size_t i=0;i<ALLOCATOR_NAME_COUNT;i++)if(!strcmp(n,allocator_names[i])) { allocator_labels[i]=address;allocator_found[i]=true; }
            for(size_t i=0;i<LOADER_NAME_COUNT;i++)if(!strcmp(n,loader_names[i])) { loader_labels[i]=address;loader_found[i]=true; }
            for(size_t i=0;i<LOADER_SEARCH_NAME_COUNT;i++)if(!strcmp(n,loader_search_names[i])) { loader_search_labels[i]=address;loader_search_found[i]=true; }
            for(size_t i=0;i<STARTUP_NAME_COUNT;i++)if(!strcmp(n,startup_names[i])) { startup_labels[i]=address;startup_found[i]=true; }
            for(size_t i=0;i<FIXTURE_NAME_COUNT;i++)if(!strcmp(n,fixture_names[i])) { fixture_labels[i]=address;fixture_found[i]=true; }
        } else if(!strncmp(line,"sym\t",4)&&(strstr(line,"type=lab")||strstr(line,"type=equ"))) {
            char *n=strstr(line,"name=\""),*v=strstr(line,"val=0x"); if(!n||!v)continue; n+=6;char *end=strchr(n,'"');if(!end)continue;*end=0;
            address=(unsigned)strtoul(v+4,NULL,16);for(size_t i=0;i<NAME_COUNT;i++)if(!strcmp(n,names[i])) { labels[i]=address;found[i]=true; }
            for(size_t i=0;i<ALLOCATOR_NAME_COUNT;i++)if(!strcmp(n,allocator_names[i])) { allocator_labels[i]=address;allocator_found[i]=true; }
            for(size_t i=0;i<LOADER_NAME_COUNT;i++)if(!strcmp(n,loader_names[i])) { loader_labels[i]=address;loader_found[i]=true; }
            for(size_t i=0;i<LOADER_SEARCH_NAME_COUNT;i++)if(!strcmp(n,loader_search_names[i])) { loader_search_labels[i]=address;loader_search_found[i]=true; }
            for(size_t i=0;i<STARTUP_NAME_COUNT;i++)if(!strcmp(n,startup_names[i])) { startup_labels[i]=address;startup_found[i]=true; }
            for(size_t i=0;i<FIXTURE_NAME_COUNT;i++)if(!strcmp(n,fixture_names[i])) { fixture_labels[i]=address;fixture_found[i]=true; }
        }
    } free(data);
}
static uint64_t (*csv(const char *path,int columns,size_t *count))[40] {
    FILE *f=fopen(path,"r");if(!f)fail("Read %s",path);char *line=NULL;size_t len=0,n=0,capacity=256;
    uint64_t (*rows)[40]=malloc(capacity*sizeof(*rows));REQUIRE(rows,"Out of memory");REQUIRE(getline(&line,&len,f)>0,"Missing CSV header");
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
static void validate_sprite_pressure_fixture_log(const char *output) {
    char *log=read_file(join(output,"run.log"),NULL),*save=NULL;
    unsigned full_setup=0,full_find=0,full_delete=0,full_owned=0,full_released=0;
    unsigned partial_setup=0,partial_find=0,partial_delete=0,partial_owned=0,partial_released=0,partial_gap=0;
    for(char *line=strtok_r(log,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
        if(sscanf(line,"VIEW fixture full: setup=%u find=%u delete=%u owned=%u released=%u",&full_setup,&full_find,&full_delete,&full_owned,&full_released)==5) continue;
        if(sscanf(line,"VIEW fixture partial: setup=%u find=%u delete=%u owned=%u released=%u gap_blocks=%u",&partial_setup,&partial_find,&partial_delete,&partial_owned,&partial_released,&partial_gap)==6) continue;
    }
    REQUIRE(full_setup==1&&full_find>0&&full_delete>0&&full_owned>0&&full_owned==full_released,"Full-pressure fixture did not execute real allocator calls and release its owned blocks");
    REQUIRE(partial_setup==1&&partial_find>0&&partial_delete>0&&partial_owned>0&&partial_owned==partial_released&&partial_gap>0,"Partial-pressure fixture did not execute real allocator calls and release its owned blocks");
    free(log);
}
static void validate_picture(const char *output) {
    size_t count;uint64_t (*rows)[40]=csv(join(output,"picture.csv"),15,&count);
    REQUIRE(count==2,"Picture test did not complete both production draws");
    REQUIRE(rows[0][0]==1&&rows[1][0]==2&&rows[0][1]==1&&rows[1][1]==1,"Picture test used an unexpected draw or resource");
    REQUIRE(rows[0][3]>0&&rows[0][3]==rows[0][4]&&!rows[0][5],"IRQ-enabled b11DrawPic did not service every asserted VSYNC");
    REQUIRE(rows[0][6]>0&&rows[0][8]==1&&rows[0][9]>0&&rows[0][10]>0,"IRQ-enabled b11DrawPic missed an owned IRQ, fill, or priority split");
    REQUIRE(rows[0][7]<4096,"Foreground picture IRQ exceeded its 4096-cycle budget");
    REQUIRE(!rows[0][11],"b11DrawPic entered with IRQs masked");
    REQUIRE(rows[1][3]>rows[1][4]&&rows[1][5]>0&&!rows[1][6]&&!rows[1][8],"IRQ-disabled reference did not suppress an owned VSYNC");
    REQUIRE(rows[1][9]>0&&rows[1][10]>0&&!rows[1][11],"Reference draw did not execute flood fill and priority split");
    REQUIRE(rows[0][13]==1&&rows[0][14]==1&&rows[1][13]==0&&rows[1][14]==1,"Picture fixture did not execute the real load.pic/draw.pic commands");
    size_t blit_count;uint64_t (*blit)[40]=csv(join(output,"agi-blit-failures.csv"),38,&blit_count);
    REQUIRE(blit_count==3&&blit[0][0]==1&&blit[1][0]==2&&blit[2][0]==3,"Native run did not complete both agiBlit failures and the retry");
    REQUIRE(blit[0][1]&&blit[0][2]&&!blit[0][3]&&blit[0][4]&&!blit[0][6],"agiBlit bESetLoop failure did not return false with its bank restored");
    REQUIRE(blit[1][5]&&!blit[1][6],"agiBlit sprite allocation failure did not return false");
    for(size_t i=0;i<2;i++) {
        REQUIRE(blit[i][7]==1&&blit[i][8]==1&&blit[i][9]==1&&blit[i][10]==0,"agiBlit view-caller ownership did not remain at depth 1 and release to 0");
        REQUIRE(blit[i][11]&&blit[i][12]&&blit[i][13],"agiBlit failure changed RAM bank, ROM bank, or I flag");
        REQUIRE(blit[i][14]==1&&blit[i][15]==1&&blit[i][16]==1&&blit[i][17]==1,"agiBlit fixture did not load, set, and draw a real view resource");
    }
    REQUIRE(blit[0][18]&&blit[0][19]&&blit[0][22]&&blit[0][23]&&blit[0][26],"Failed bESetLoop published loop addresses or changed backbuffer metadata");
    REQUIRE(!blit[1][18]&&!blit[1][19]&&blit[1][20]&&blit[1][21]&&blit[1][22]&&blit[1][23]&&blit[1][26],"C allocation failure did not retain reserved loop addresses without publishing a backbuffer");
    REQUIRE(blit[2][27]&&blit[2][28]&&blit[2][30]&&blit[2][31]&&blit[2][32]&&blit[2][33]&&blit[2][34]&&blit[2][35]&&blit[2][36]&&blit[2][37],"agiBlit retry did not retain prior cel reservations and commit valid active loop/backbuffer state");
    validate_sprite_pressure_fixture_log(output);
    enum { VIEW_ID,FAIL_LOAD_COMMAND,FAIL_CALLER,FAIL_LOADER,FAIL_LOADER_RETURNED,FAIL_LOADER_FALSE,FAIL_CALLER_RETURNED,FAIL_VIEW_UNCHANGED,FAIL_METADATA_UNCHANGED,FAIL_ALLOCATOR_UNCHANGED,RETRY_LOAD_COMMAND,RETRY_SET_COMMAND,RETRY_DRAW_COMMAND,RETRY_CALLER,RETRY_LOADER,RETRY_LOADER_RETURNED,RETRY_LOADER_TRUE,RETRY_VIEW_LOADED,RETRY_BLIT,RETRY_BANK,RETRY_ROM,RETRY_I,RETRY_DEPTH,RETRY_LOOP_VALID,RETRY_LOOP_RESERVED,RETRY_BACKBUFFER_VALID,RETRY_BACKBUFFER_RESERVED,RETRY_METADATA_COMMITTED };
    size_t view_retry_count;uint64_t (*view_retry)[40]=csv(join(output,"view-load-retry.csv"),28,&view_retry_count);
    REQUIRE(view_retry_count==1,"Native run did not complete the production view caller failure and retry");
    REQUIRE(view_retry[0][FAIL_LOAD_COMMAND]==1&&view_retry[0][FAIL_CALLER]==1&&view_retry[0][FAIL_LOADER]==1&&view_retry[0][FAIL_LOADER_RETURNED]&&view_retry[0][FAIL_LOADER_FALSE]&&view_retry[0][FAIL_CALLER_RETURNED],"The actual load.view caller did not receive a failed b6LoadAGIFile result");
    REQUIRE(view_retry[0][FAIL_VIEW_UNCHANGED]&&view_retry[0][FAIL_METADATA_UNCHANGED]&&view_retry[0][FAIL_ALLOCATOR_UNCHANGED],"Failed load.view changed the cold View row, metadata, or sprite reservations");
    REQUIRE(view_retry[0][RETRY_LOAD_COMMAND]==1&&view_retry[0][RETRY_SET_COMMAND]==1&&view_retry[0][RETRY_DRAW_COMMAND]==1&&view_retry[0][RETRY_CALLER]==1&&view_retry[0][RETRY_LOADER]==1&&view_retry[0][RETRY_LOADER_RETURNED]&&view_retry[0][RETRY_LOADER_TRUE]&&view_retry[0][RETRY_VIEW_LOADED],"The restored directory did not let real load.view/set.view/draw succeed on retry");
    REQUIRE(view_retry[0][RETRY_BLIT]&&view_retry[0][RETRY_BANK]&&view_retry[0][RETRY_ROM]&&view_retry[0][RETRY_I]&&view_retry[0][RETRY_DEPTH],"The retried view blit did not return successfully with caller state restored");
    REQUIRE(view_retry[0][RETRY_LOOP_VALID]&&view_retry[0][RETRY_LOOP_RESERVED]&&view_retry[0][RETRY_BACKBUFFER_VALID]&&view_retry[0][RETRY_BACKBUFFER_RESERVED]&&view_retry[0][RETRY_METADATA_COMMITTED],"The retried view did not commit valid, reserved loop and backbuffer addresses");
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
    fprintf(stderr,"Production agiBlit retry: real load.view/set.view/draw handlers ran; bESetLoop failure published no loop addresses; C allocation failure retained reserved loop addresses without publishing a backbuffer; the next frame retried successfully and committed reserved backbuffers; owner depth and CPU state preserved across the caller return\n");
    fprintf(stderr,"Production view loader retry: view %llu reached b9LoadViewFile and received b6LoadAGIFile false; its complete cold View row, metadata, and sprite table were unchanged; the restored directory retried load.view/set.view/draw and committed reserved loop/backbuffer addresses\n",(unsigned long long)view_retry[0][VIEW_ID]);
    free(irq);free(reference);free(rows);free(blit);free(view_retry);
}
/* Regression reference from the real combined KQ1 case: image SHA256
 * 8b083550c6cb24248b1c0dcb52b0f352653bb4c3919f84910b064598b229ec06,
 * ROM SHA256 b81654cc8c87ed96e3ffc7c8e7c312c9f3b7b870c7bb34de61e61eac931b819a,
 * emulator source 77f2bab332e24c5456f9844a6c7ddd203f6198e6. Workload has
 * 6 real menus, 4 active sound channels, 5 sprite records and 513 text bytes;
 * measured rows 29987/4089 cycles, total 34076, max acceptance-to-RTI
 * I-masked 29987 cycles, completed in 2 VSYNCs. This is a regression reference,
 * not a before/after speed claim. */
enum {
    COMBINED_FRAME_CYCLES = 134400,
    COMBINED_MAX_DISPATCH_CYCLES = 50000,
    COMBINED_MAX_I_MASKED_CYCLES = 50000,
    COMBINED_MAX_TOTAL_IRQ_CYCLES = 100000,
    COMBINED_MAX_COMPLETION_FRAMES = 2,
    COMBINED_BASELINE_FIRST_ROW_CYCLES = 29987,
    COMBINED_BASELINE_TOTAL_IRQ_CYCLES = 34076,
    COMBINED_BASELINE_MAX_I_MASKED_CYCLES = 29987,
    COMBINED_BASELINE_MARGIN_CYCLES = 4096
};
static void validate_text_case_log(const char *output) {
    char *path=join(output,"run.log"),*contents=read_file(path,NULL),*save=NULL;
    free(path);
    const char *multiply_marker="MULT_8x8_16 boundary grid passed: 81 products checked with 16-bit oracle";
    unsigned multiply_count=0,publisher_count=0,case_count=0;
    unsigned long long source_reads=0,out_of_extent_reads=0;
    for(char *line=strtok_r(contents,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
        if(!strcmp(line,multiply_marker)) {
            if(multiply_count||publisher_count||case_count) fail("Combined log contains a duplicate or late multiply-grid marker");
            multiply_count++;
            continue;
        }
        if(strstr(line,"Blocking display-text publisher returned after two production IRQ slices:")) {
            if(publisher_count||case_count) fail("Combined log contains a duplicate or late blocking-publisher completion marker");
            if(multiply_count!=1) fail("Blocking-publisher completion preceded the multiply-grid result check");
            publisher_count++;
            continue;
        }
        if(!strstr(line,"Production text case ")||!strstr(line," passed:")) continue;
        if(!publisher_count) fail("Text-case pass marker preceded the blocking-publisher completion marker");
        unsigned index,chars,case_reads,case_oob,c000,generation,last_address,last_cycle,completion; char name[64];
        int fields=sscanf(line,"Production text case %u (%63[^)]) passed: chars=%u source_reads=%u oob=%u C000=%u generation=%x last_store=%x@%u completion=%u",&index,name,&chars,&case_reads,&case_oob,&c000,&generation,&last_address,&last_cycle,&completion);
        if(fields!=10) fail("Malformed production text-case pass marker");
        static const char *const expected_names[]={"owned-buffer-offset-1999-unterminated-byte","bank-alias-same-address","source-stops-at-c000","full-1856-cell-clear","overlapping-120-character-copy"};
        if(case_count>=5||index!=case_count||strcmp(name,expected_names[index])) fail("Production text-case pass markers are duplicated, misnamed, or out of order");
        source_reads+=case_reads;
        out_of_extent_reads+=case_oob;
        fprintf(stderr,"  %s\n",line);
        case_count++;
    }
    free(contents);
    REQUIRE(multiply_count==1,"Combined run did not verify all 81 multiply-boundary products exactly once");
    REQUIRE(publisher_count==1,"Combined run did not return from the blocking display-text publisher exactly once");
    REQUIRE(case_count==5,"Combined run did not complete all five production text cases in order");
    REQUIRE(out_of_extent_reads==0,"Production text cases read beyond their owned source extents");
    fprintf(stderr,"MULT_8x8_16 boundary grid: 81/81 full 16-bit products verified.\n");
    fprintf(stderr,"Production text cases: %u/5 completed; source reads=%llu, out-of-extent reads=%llu.\n",case_count,source_reads,out_of_extent_reads);
}
static void validate_combined(const char *output) {
    size_t count; uint64_t (*rows)[40]=csv(join(output,"combined.csv"),21,&count);
    REQUIRE(count==2,"Combined IRQ did not complete its initial and deferred production rows");
    uint64_t total=rows[0][1]+rows[1][1],max_masked=rows[0][2]>rows[1][2]?rows[0][2]:rows[1][2];
    fprintf(stderr,"Combined measured rows: %llu/%llu cycles, total %llu; I-masked %llu/%llu cycles; completion %llu frames, asserted/serviced %llu/%llu.\n",
        (unsigned long long)rows[0][1],(unsigned long long)rows[1][1],(unsigned long long)total,
        (unsigned long long)rows[0][2],(unsigned long long)rows[1][2],(unsigned long long)rows[0][16],
        (unsigned long long)rows[0][17],(unsigned long long)rows[0][18]);
    fprintf(stderr,"Combined acceptance-to-RTI versus reported I-masked interval: row1=%llu/%llu row2=%llu/%llu cycles.\n",
        (unsigned long long)rows[0][1],(unsigned long long)rows[0][2],
        (unsigned long long)rows[1][1],(unsigned long long)rows[1][2]);
    REQUIRE(rows[0][1]==rows[0][2]&&rows[1][1]==rows[1][2],
            "Combined I-masked measurement omitted the accepted IRQ prefix before the custom handler");
    REQUIRE(rows[0][0]==1&&rows[1][0]==2,"Combined IRQ row order changed");
    REQUIRE(rows[0][3]==15,"Sound, menu, sprite and text work did not all execute in the same IRQ row");
    REQUIRE(rows[0][4]>0&&rows[0][5]>0&&rows[0][6]>0,"Combined row did not play active KQ1 sound through PSG writes");
    REQUIRE(rows[0][7]>0&&rows[0][8]>0,"Combined row did not render the real KQ1 menu");
    REQUIRE(rows[0][9]>0&&rows[0][10]>0,"Combined row did not consume a real KQ1 sprite update");
    REQUIRE(rows[0][11]==512&&rows[0][12]==4&&rows[0][13]==0,"Combined row did not execute the first real text slice");
    REQUIRE((rows[1][3]&8)&&rows[1][11]==1&&rows[1][12]==0&&rows[1][13]==2,"Deferred IRQ row did not finish the final text byte");
    REQUIRE(rows[0][19]==0&&rows[1][19]==0&&rows[0][20]==1&&rows[1][20]==1,"Blocking publisher did not dispatch both text slices with IRQ entry depth zero and I set");
    REQUIRE(rows[0][14]==total&&rows[0][15]==max_masked&&rows[0][16]==2,"Combined IRQ summary disagrees with its measured rows");
    REQUIRE(rows[0][16]==rows[0][17]&&rows[0][17]==rows[0][18],"Combined workload lost or duplicated VSYNC time");
    REQUIRE(rows[0][16]<=COMBINED_MAX_COMPLETION_FRAMES,"Deferred text exceeded its VSYNC completion limit");
    REQUIRE(rows[0][1]<COMBINED_FRAME_CYCLES&&rows[1][1]<COMBINED_FRAME_CYCLES&&total<2*COMBINED_FRAME_CYCLES,"Combined IRQ exceeded the per-frame CPU-cycle bound");
    REQUIRE(max_masked<COMBINED_FRAME_CYCLES,"Combined IRQ exceeded the continuous I-masked frame bound");
    REQUIRE(rows[0][1]<COMBINED_MAX_DISPATCH_CYCLES&&rows[1][1]<COMBINED_MAX_DISPATCH_CYCLES&&total<COMBINED_MAX_TOTAL_IRQ_CYCLES,"Combined IRQ exceeded its explicit KQ1 cycle limits");
    REQUIRE(max_masked<COMBINED_MAX_I_MASKED_CYCLES,"Combined IRQ exceeded its explicit I-masked limit");
    REQUIRE(rows[0][1]<=COMBINED_BASELINE_FIRST_ROW_CYCLES+COMBINED_BASELINE_MARGIN_CYCLES,"Combined first-row cycles regressed from the retained baseline");
    REQUIRE(total<=COMBINED_BASELINE_TOTAL_IRQ_CYCLES+COMBINED_BASELINE_MARGIN_CYCLES,"Combined total IRQ cycles regressed from the retained baseline");
    REQUIRE(max_masked<=COMBINED_BASELINE_MAX_I_MASKED_CYCLES+COMBINED_BASELINE_MARGIN_CYCLES,"Combined continuous I-masked interval regressed from the retained baseline");
    fprintf(stderr,"Combined KQ1 workload: %llu real menus, %llu active sound channel(s), %llu game-produced sprite record(s), 513 text bytes; dispatch rows %llu/%llu cycles, total %llu IRQ cycles, max continuous I-masked %llu cycles, completion %llu VSYNCs (asserted/serviced %llu/%llu). Not maximum-menu coverage. Baseline limits: first row %u + %u, total %u + %u, I-masked %u + %u cycles.\n",
        (unsigned long long)rows[0][7],(unsigned long long)rows[0][4],(unsigned long long)rows[0][9],
        (unsigned long long)rows[0][1],(unsigned long long)rows[1][1],(unsigned long long)total,(unsigned long long)max_masked,
        (unsigned long long)rows[0][16],(unsigned long long)rows[0][17],(unsigned long long)rows[0][18],
        COMBINED_BASELINE_FIRST_ROW_CYCLES,COMBINED_BASELINE_MARGIN_CYCLES,COMBINED_BASELINE_TOTAL_IRQ_CYCLES,COMBINED_BASELINE_MARGIN_CYCLES,
        COMBINED_BASELINE_MAX_I_MASKED_CYCLES,COMBINED_BASELINE_MARGIN_CYCLES);
    validate_text_case_log(output);
    free(rows);
}
static void validate(const char *output,bool foreground_test) {
    size_t n,ncmd;uint64_t (*rows)[40]=csv(join(output,"irq.csv"),23,&n),(*commands)[40]=csv(join(output,"commands.csv"),11,&ncmd);
    REQUIRE(ncmd==5,"Missing commands");const int opcode[]={98,99,110,110,110},operand[]={0,0,1,0,2};
    for(size_t i=0;i<5;i++) { REQUIRE(commands[i][0]==(unsigned)opcode[i]&&commands[i][1]==(unsigned)operand[i],"Unexpected injected command");if(i==2) REQUIRE(commands[i][4]==33,"Menu command VSYNC count changed");else if(i==4) REQUIRE(commands[i][4]==65,"Status redraw command VSYNC count changed");else REQUIRE(commands[i][4]>=16&&commands[i][4]<33,"Idle command exceeded the integration frame window");REQUIRE(commands[i][5]&&commands[i][6]&&commands[i][7],"Restoration witness missing"); }
    size_t active=0,writes=0;uint64_t maxirq=0,maxshake=0,maxwrite=0,calls=0;bool sound=false,sprite=false,input=false;
    for(size_t i=0;i<n;i++) { uint64_t *r=rows[i];if(r[1]>maxirq)maxirq=r[1];if(!r[5])continue;active++;if(r[1]>maxshake)maxshake=r[1];calls|=r[8];sound|=r[7]&&r[13];sprite|=r[14]!=0;input|=r[15]!=0;if(r[10]) { writes++;if(r[12]>maxwrite)maxwrite=r[12]; } }
    REQUIRE(active&&writes,"No full-game shake activity");REQUIRE(maxwrite<11520,"Window write missed VGA blanking");REQUIRE(sound,"No actual playing PSG channels during shake");REQUIRE(sprite,"No actual sprite attribute writes during shake");REQUIRE(input,"No nonempty input-line redraw during shake");REQUIRE(commands[2][10]>0,"Keyboard was not queued while foreground waited");REQUIRE((calls&63)==63,"Required IRQ paths not observed");
    if(foreground_test) {
        size_t foreground_rows;uint64_t (*foreground)[40]=csv(join(output,"foreground.csv"),8,&foreground_rows);
        size_t picture_calls=0,object_calls=0,picture_vsyncs=0,object_vsyncs=0;uint64_t max_cycles=0,max_busy_irq=0,busy_irqs=0,object_busy_irqs=0;
        for(size_t i=0;i<foreground_rows;i++) {
            if(foreground[i][0]==1) { picture_calls++;picture_vsyncs+=foreground[i][2]; }
            else if(foreground[i][0]==2) { object_calls++; if(foreground[i][2]) { object_vsyncs+=foreground[i][2]; object_busy_irqs+=foreground[i][5]; } }
            REQUIRE(foreground[i][4]==0,"Foreground caller lost a production VSYNC");
            if(foreground[i][1]>max_cycles)max_cycles=foreground[i][1];
            busy_irqs+=foreground[i][5];if(foreground[i][6]>max_busy_irq)max_busy_irq=foreground[i][6];
            REQUIRE(foreground[i][7]==0,"Foreground caller entered with IRQs masked");
        }
        REQUIRE(picture_calls&&picture_vsyncs,"Warm-up did not render the real KQ1 picture through b11DrawPic");
        REQUIRE(object_calls&&object_vsyncs&&object_busy_irqs,"No actual b9UpdateObjects workload crossed a VSYNC with a busy IRQ");
        fprintf(stderr,"Production renderer timing: b11DrawPic calls=%zu, b9UpdateObjects calls=%zu, picture VSYNCs=%zu, max caller cycles=%llu, busy IRQs=%llu, max busy IRQ cycles=%llu\n",picture_calls,object_calls,picture_vsyncs,(unsigned long long)max_cycles,(unsigned long long)busy_irqs,(unsigned long long)max_busy_irq);
        /* Whole IRQ includes ROM keyboard work plus the separately bounded sound path. */
        REQUIRE(max_busy_irq<4096+2000,"Foreground IRQ exceeded ROM plus sound CPU budget");
        free(foreground);
    }
    uint64_t original[4]={0},original_border=0;
    for(size_t c=2;c<5;c++) {
        unsigned count=(unsigned)commands[c][1];REQUIRE(commands[c][8]==count*32,"Incorrect command window writes");if(!count)continue;
        size_t last=0,total=0;for(size_t i=0;i<n;i++)if(rows[i][0]>commands[c][2]&&rows[i][0]<commands[c][3]) {last=i;total++;}
        REQUIRE(total==count*32+1,"Incorrect phase sample count");for(int k=0;k<4;k++)original[k]=rows[last][16+k];original_border=rows[last][20];size_t index=0;
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
    size_t nvideo;uint64_t (*video)[40]=csv(join(output,"video-frames.csv"),18,&nvideo);REQUIRE(nvideo==frames,"GIF frame trace does not cover every coalesced image");
    for(size_t i=0;i<nvideo;i++) REQUIRE(video[i][0]==i,"GIF frame trace index is not aligned with coalesced images");
    int vertical=original[2]==0?7:8;size_t pair=0;
    for(size_t i=16;i+4<frames;i++) { unsigned char *shifted=images+i*framebytes,*normal=images+(i+4)*framebytes;
        if(crop_equal(normal,shifted,width,32,128,592,360,16,vertical)&&crop_equal(normal,shifted,width,16,0,590,72,16,vertical)&&!crop_equal(normal,shifted,width,32,128,592,360,0,0)) {pair=i;break;} }
    REQUIRE(pair,"No coherent bitmap/sprite/L1 movement in real game frames");
    uint64_t final_command_exit=commands[4][3];size_t restore_before=SIZE_MAX,restore_after=SIZE_MAX;
    for(size_t i=0;i<nvideo;i++) { if(video[i][2]<final_command_exit) restore_before=i; else if(restore_after==SIZE_MAX) restore_after=i; }
    REQUIRE(restore_before!=SIZE_MAX&&restore_after!=SIZE_MAX,"No rendered frames bracketed the final shake command return");
    REQUIRE(restore_after==restore_before+1,"Final shake return was not bracketed by adjacent rendered frames");
    REQUIRE(video[restore_before][2]<final_command_exit&&video[restore_after][2]>=final_command_exit,"Frame timestamps do not bracket the final shake return");
    REQUIRE(video[restore_before][4]==4&&video[restore_before][5]==1&&video[restore_before][6]==1,"Last pre-return frame was not the final unshifted shake phase");
    REQUIRE(video[restore_after][4]==5&&video[restore_after][5]==0&&video[restore_after][6]==0,"First post-return frame did not show the completed shake state");
    for(int k=0;k<4;k++) REQUIRE(video[restore_before][8+k]==original[k]&&video[restore_after][8+k]==original[k],"Shake return frames did not restore the original display window");
    REQUIRE(video[restore_before][12]==original_border&&video[restore_after][12]==original_border,"Shake return frames did not restore the original border");
    REQUIRE(video[restore_before][15]==video[restore_after][15],"b9UpdateObjects resumed before the first post-shake frame");
    REQUIRE(!video[restore_before][16]&&!video[restore_after][16],"A foreground renderer was active at a shake return frame");
    const char *pngnames[]={"before.png","shifted.png","original_phase.png","restored.png","final.png"};size_t indices[]={2,pair,pair+4,restore_after,frames-1};for(int i=0;i<5;i++)save_png(join(output,pngnames[i]),images+indices[i]*framebytes,width,height);
    fprintf(stderr,"Visual restoration compares last pre-return frame %zu (cycle %llu, b9=%llu) with first post-return frame %zu (cycle %llu, b9=%llu); final command exit=%llu, final capture frame=%zu\n",restore_before,(unsigned long long)video[restore_before][2],(unsigned long long)video[restore_before][15],restore_after,(unsigned long long)video[restore_after][2],(unsigned long long)video[restore_after][15],(unsigned long long)final_command_exit,frames-1);
    REQUIRE(crop_equal(images+restore_before*framebytes,images+restore_after*framebytes,width,32,128,592,360,0,0),"Game picture did not visually restore before foreground animation resumed");
    char *log=read_file(join(output,"run.log"),NULL);REQUIRE(strstr(log,"Keyboard processed after resume: 1"),"Queued key not processed after resume");free(log);
    char *reportpath=join(output,"report.json");FILE *report=fopen(reportpath,"w");REQUIRE(report,"Cannot write report");char *q=quoted(output);
    fprintf(report,"{\n  \"output\": %s,\n  \"irq_samples\": %zu,\n  \"active_shake_samples\": %zu,\n  \"irq_cycles_max\": %llu,\n  \"shake_irq_cycles_max\": %llu,\n  \"vsync_to_window_write_cycles_max\": %llu,\n  \"active_calls_mask\": %llu,\n  \"commands\": [\n",q,n,active,(unsigned long long)maxirq,(unsigned long long)maxshake,(unsigned long long)maxwrite,(unsigned long long)calls);free(q);
    const char *keys[]={"opcode","operand","entry_cycle","exit_cycle","vsyncs","display_restored","code_window_restored","cursor_restored","window_writes","actual_command_entered","kbd_queue"};
    for(int i=0;i<5;i++) {fprintf(report,"    {");for(int k=0;k<11;k++)fprintf(report,"%s\"%s\": \"%llu\"",k?", ":"",keys[k],(unsigned long long)commands[i][k]);fprintf(report,"}%s\n",i==4?"":",");}
    fprintf(report,"  ],\n  \"gif_frames\": %zu,\n  \"visual_pair\": [%zu, %zu],\n  \"visual_restoration_frames\": [%zu, %zu],\n  \"visual_restoration_cycles\": [%llu, %llu],\n  \"visual_restoration_b9_entries\": [%llu, %llu],\n  \"visual_physical_displacement\": [16, %d],\n  \"keyboard_queued_during_shake\": true,\n  \"keyboard_processed_after_resume\": true,\n  \"limits\": \"Finite KQ1 sample with forced dirty menu/score; native VIEW 33 load/animate/set/position/draw produces a nonempty sprite batch that is replayed through the real IRQ handler. Temporary LOOK input fixture. Not all-game worst case or physical hardware. This emulator renders the 0->4 VSTART change as 7 pixels due to its eff_y_fp initialization; registers retain correct hardware units.\"\n}\n",frames,pair,pair+4,restore_before,restore_after,(unsigned long long)video[restore_before][2],(unsigned long long)video[restore_after][2],(unsigned long long)video[restore_before][15],(unsigned long long)video[restore_after][15],vertical);REQUIRE(!fclose(report),"Report close failed");char *result=read_file(reportpath,NULL);puts(result);free(result);free(images);free(rows);free(commands);free(video);
}
enum { LOADER_SEARCH_COUNT = 9 };
typedef struct {
    unsigned init_entries,init_returned,init_result,words_entries,words_returned,words_result;
    unsigned open_entries,truncated,alloc_entries,published_unchanged,text_unchanged,allocator_unchanged;
    unsigned event_entries,interpreter_entries,irq_entries,object_entries;
    unsigned pass_entries,pass_returns,pass_result,read_calls,read_bytes,read_a,read_x,seek_calls,seek_a,seek_x;
    unsigned loaded_words,search_count,allocator_before_count,allocator_after_count,allocator_added,allocator_lost;
    unsigned exit_seen,exit_status,exit_ram_bank,exit_code_bank;
    int search_results[LOADER_SEARCH_COUNT];
} LoaderResult;
static LoaderResult read_loader_result(const char *path) {
    LoaderResult r={0};char *data=read_file(path,NULL);
    int fields=sscanf(data,"%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %d %d %d %d %d %d %d %d %d %u %u %u %u",
        &r.init_entries,&r.init_returned,&r.init_result,&r.words_entries,&r.words_returned,
        &r.words_result,&r.open_entries,&r.truncated,&r.alloc_entries,&r.published_unchanged,
        &r.text_unchanged,&r.allocator_unchanged,&r.event_entries,&r.interpreter_entries,&r.irq_entries,&r.object_entries,
        &r.pass_entries,&r.pass_returns,&r.pass_result,&r.read_calls,&r.read_bytes,&r.read_a,&r.read_x,
        &r.seek_calls,&r.seek_a,&r.seek_x,&r.loaded_words,&r.search_count,
        &r.allocator_before_count,&r.allocator_after_count,&r.allocator_added,&r.allocator_lost,
        &r.search_results[0],&r.search_results[1],&r.search_results[2],&r.search_results[3],
        &r.search_results[4],&r.search_results[5],&r.search_results[6],&r.search_results[7],&r.search_results[8],
        &r.exit_seen,&r.exit_status,&r.exit_ram_bank,&r.exit_code_bank);
    free(data);REQUIRE(fields==45,"Incomplete real-emulator loader/search report");return r;
}
static bool validate_loader_result(const char *name,const char *path,bool words,unsigned opens,
                                   unsigned allocations,bool truncated,unsigned expected_passes,unsigned objects,
                                   bool success,unsigned expected_words,const int *search_results,
                                   bool expect_fatal_object_exit) {
    LoaderResult r=read_loader_result(path);bool okay=true;
#define LOADER_CHECK(condition,message) do { if(!(condition)) { fprintf(stderr,"FAIL loader %s: %s\n",name,message);okay=false; } } while(0)
    LOADER_CHECK(r.init_entries==1,"b6Initialise was not entered through the observed startup call");
    if(expect_fatal_object_exit) {
        LOADER_CHECK(!r.init_returned,"fatal OBJECT startup unexpectedly returned from b6Initialise");
        LOADER_CHECK(r.exit_seen&&r.exit_status!=0&&r.exit_code_bank==0,
                     "missing OBJECT did not enter resident _exit with a nonzero 16-bit status");
        if(r.exit_seen) fprintf(stderr,"loader %s resident _exit: status=%04x RAM bank=%02x code bank=%02x\n",
                                name,r.exit_status,r.exit_ram_bank,r.exit_code_bank);
    } else {
        LOADER_CHECK(r.init_returned,"b6Initialise did not return through the observed startup call");
        LOADER_CHECK(!r.exit_seen,"loader observed an unexpected fatal _exit before b6Initialise returned");
    }
    LOADER_CHECK(r.object_entries==objects,"startup did not reach the expected real OBJECT loader");
    LOADER_CHECK(r.words_entries==(words?1u:0u),"startup did not reach the expected real WORDS loader");
    if(words) {
        fprintf(stderr,"loader %s I/O: passes=%u returned=%u result=%u reads=%u bytes=%u alloc=%u live=%u (+%u) lastAX=%02x%02x seeks=%u lastAX=%02x%02x\n",name,r.pass_entries,r.pass_returns,r.pass_result,r.read_calls,r.read_bytes,r.alloc_entries,r.allocator_after_count,r.allocator_added,r.read_x,r.read_a,r.seek_calls,r.seek_x,r.seek_a);
        LOADER_CHECK(r.words_returned,"b12LoadWords did not return after the real loader failure");
        LOADER_CHECK(r.words_result==(success?1u:0u),success?"b12LoadWords rejected the generated real WORDS.TOK":"b12LoadWords reported success after a real loader failure");
        LOADER_CHECK(r.pass_entries==expected_passes&&r.pass_returns==expected_passes,
                     "real WORDS parser pass count did not match the fixture");
        if(expected_passes) LOADER_CHECK(r.pass_result==(success?1u:0u),
                                         success?"real count and publish passes did not both complete successfully":"real WORDS parser did not return the expected failure");
        if(r.open_entries!=opens||r.truncated!=(unsigned)truncated) { fprintf(stderr,"FAIL loader %s: opens=%u truncated=%u expected=%u/%u\n",name,r.open_entries,r.truncated,opens,(unsigned)truncated); okay=false; }
        if(r.alloc_entries!=allocations) { fprintf(stderr,"FAIL loader %s: allocator calls=%u expected=%u\n",name,r.alloc_entries,allocations); okay=false; }
        if(!success) {
            LOADER_CHECK(r.published_unchanged,"failed WORDS load changed published counts or pointers");
            LOADER_CHECK(r.text_unchanged,"failed WORDS load changed the published wordsTextStart table");
            LOADER_CHECK(r.allocator_unchanged,"failed WORDS load changed the target banked allocation map");
        }
    }
    if(!success&&(r.event_entries||r.interpreter_entries||r.irq_entries)) {
        fprintf(stderr,"FAIL loader %s: initialization proceeded after startup failure (events/interpreter/IRQ=%u/%u/%u, b6Initialise result=%u)\n",
                name,r.event_entries,r.interpreter_entries,r.irq_entries,r.init_result);okay=false;
    }
    if(success) {
        LOADER_CHECK(r.init_result==1,"b6Initialise did not return success after loading the real WORDS.TOK");
        LOADER_CHECK(r.words_result==1,"b12LoadWords did not return success for the real WORDS.TOK");
        LOADER_CHECK(r.loaded_words==expected_words,"target WORDS word count did not match the generated file");
        LOADER_CHECK(r.search_count==LOADER_SEARCH_COUNT,"compiled target search wrapper did not run every WORDS query");
        if(r.allocator_added!=3||r.allocator_lost||r.allocator_after_count!=r.allocator_before_count+3) {
            fprintf(stderr,"FAIL loader %s: live WORDS allocations baseline=%u after=%u added=%u lost=%u; expected exactly three retained allocations and no baseline releases\n",
                    name,r.allocator_before_count,r.allocator_after_count,r.allocator_added,r.allocator_lost);okay=false;
        }
        if(search_results) for(unsigned i=0;i<LOADER_SEARCH_COUNT;i++) if(r.search_results[i]!=search_results[i]) { fprintf(stderr,"FAIL loader %s: search %u returned %d expected %d\n",name,i,r.search_results[i],search_results[i]);okay=false; }
        LOADER_CHECK(r.event_entries==1&&r.interpreter_entries==1&&r.irq_entries==1,"successful b6Initialise did not proceed through events, interpreter, and IRQ setup");
    } else {
        if(!expect_fatal_object_exit) LOADER_CHECK(r.init_result==0,"b6Initialise did not return failure");
        LOADER_CHECK(r.search_count==0,"failed startup unexpectedly invoked target WORDS searches");
    }
    if(okay) fprintf(stderr,success?"PASS loader %s: real WORDS load completed and compiled target search results matched\n":"PASS loader %s: target failure preserved WORDS state and stopped startup before events/interpreter/IRQ\n",name);
#undef LOADER_CHECK
    return okay;
}
static void write_overcapacity_words(const char *path) {
    enum { WORD_LIMIT=4500/3, WORDS_TO_WRITE=WORD_LIMIT+1 };
    FILE *file=fopen(path,"wb");if(!file) fail("Write %s: %s",path,strerror(errno));
    TestWordsFixtureWriter writer;REQUIRE(test_words_fixture_begin(&writer,file),"Begin over-capacity WORDS fixture failed");
    for(unsigned n=0;n<WORDS_TO_WRITE;n++) {
        char word[5]={'a',(char)('a'+n/(26*26)),(char)('a'+(n/26)%26),(char)('a'+n%26),0};
        REQUIRE(test_words_fixture_add(&writer,word,0),"Write over-capacity WORDS word failed");
    }
    REQUIRE(test_words_fixture_finish(&writer),"Finish over-capacity WORDS fixture failed");
    REQUIRE(!fclose(file),"Close over-capacity WORDS failed");
}
static void write_words_search_fixture(const char *path,const char *kind) {
    FILE *file=fopen(path,"wb");if(!file) fail("Write %s: %s",path,strerror(errno));
    TestWordsFixtureWriter writer;REQUIRE(test_words_fixture_begin(&writer,file),"Begin WORDS search fixture failed");
    if(!strcmp(kind,"single")) {
        REQUIRE(test_words_fixture_add(&writer,"ant",37),"Write single-word WORDS fixture failed");
    } else if(!strcmp(kind,"prefix")) {
        REQUIRE(test_words_fixture_add(&writer,"ant",11)&&test_words_fixture_add(&writer,"ante",12)&&test_words_fixture_add(&writer,"ante",12)&&test_words_fixture_add(&writer,"anthill",13),"Write prefix WORDS fixture failed");
    } else if(!strcmp(kind,"large")) {
        for(unsigned index=0;index<513;index++) {
            char word[4]={(char)('a'+index/(26*26)),(char)('a'+(index/26)%26),(char)('a'+index%26),0};
            REQUIRE(test_words_fixture_add(&writer,word,(uint16_t)index),"Write 513-word WORDS fixture failed");
        }
    } else if(strcmp(kind,"empty")) {
        fail("Unknown WORDS search fixture: %s",kind);
    }
    REQUIRE(test_words_fixture_finish(&writer),"Finish WORDS search fixture failed");
    REQUIRE(!fclose(file),"Close WORDS search fixture failed");
}
static void patch_words_header_offset(FILE *file,unsigned letter,uint16_t offset) {
    uint8_t bytes[2]={(uint8_t)(offset>>8),(uint8_t)offset};
    REQUIRE(letter<26&&fseek(file,(long)(letter*2),SEEK_SET)==0,"Seek WORDS.TOK header offset failed");
    REQUIRE(fwrite(bytes,1,sizeof(bytes),file)==sizeof(bytes),"Patch WORDS.TOK header offset failed");
}
static void write_malformed_words_fixture(const char *path,const char *kind) {
    FILE *file;
    if(!strcmp(kind,"word-order")) {
        file=fopen(path,"w+b");if(!file) fail("Write %s: %s",path,strerror(errno));
        REQUIRE(test_words_fixture_write_descending_pair(file),"Write descending full-word-order WORDS fixture failed");
        REQUIRE(!fclose(file),"Close descending full-word-order WORDS fixture failed");
        return;
    }
    if(!strcmp(kind,"short-header")) {
        uint8_t bytes[TEST_WORDS_FIXTURE_HEADER_SIZE-1]={0};
        file=fopen(path,"wb");if(!file) fail("Write %s: %s",path,strerror(errno));
        REQUIRE(fwrite(bytes,1,sizeof(bytes),file)==sizeof(bytes),"Write truly short WORDS.TOK header failed");
        REQUIRE(!fclose(file),"Close short WORDS.TOK header failed");
        return;
    }
    if(!strcmp(kind,"header-order")) {
        TestWordsFixtureWriter writer;
        file=fopen(path,"wb");if(!file) fail("Write %s: %s",path,strerror(errno));
        REQUIRE(test_words_fixture_begin(&writer,file)&&test_words_fixture_add(&writer,"ant",1)&&
                test_words_fixture_add(&writer,"dog",2)&&test_words_fixture_finish(&writer),
                "Write ordered-offset WORDS.TOK fixture failed");
        REQUIRE(!fclose(file),"Close ordered-offset WORDS.TOK fixture failed");
        file=fopen(path,"r+b");if(!file) fail("Open %s: %s",path,strerror(errno));
        patch_words_header_offset(file,3,TEST_WORDS_FIXTURE_HEADER_SIZE);
        REQUIRE(!fclose(file),"Close patched WORDS.TOK header failed");
        return;
    }
    write_words_search_fixture(path,"single");
    file=fopen(path,"r+b");if(!file) fail("Open %s: %s",path,strerror(errno));
    if(!strcmp(kind,"excess-prefix")) {
        REQUIRE(fseek(file,TEST_WORDS_FIXTURE_HEADER_SIZE,SEEK_SET)==0&&fputc(1,file)==1,
                "Patch excessive WORDS.TOK prefix failed");
    } else if(!strcmp(kind,"header-offset")) {
        patch_words_header_offset(file,0,TEST_WORDS_FIXTURE_HEADER_SIZE-1);
    } else if(!strcmp(kind,"truncated-synonym")) {
        REQUIRE(fseek(file,0,SEEK_END)==0,"Seek WORDS.TOK synonym fixture failed");
        long size=ftell(file);
        REQUIRE(size>(long)TEST_WORDS_FIXTURE_HEADER_SIZE&&ftruncate(fileno(file),size-1)==0,
                "Truncate WORDS.TOK synonym fixture failed");
    } else {
        fail("Unknown malformed WORDS.TOK fixture: %s",kind);
    }
    REQUIRE(!fclose(file),"Close malformed WORDS.TOK fixture failed");
}
static char stage_cleanup_path[PATH_MAX];
static void cleanup_stage(void) {
    if (!stage_cleanup_path[0]) return;
    if (!suite_test_paths_remove(stage_cleanup_path)) {
        fprintf(stderr,"Could not clean game fixture directory %s\n",stage_cleanup_path);
        _Exit(EXIT_FAILURE);
    }
    stage_cleanup_path[0]=0;
}
static char *create_stage(const char *name) {
    char *stage_name=format("stage-%s-XXXXXX",name);
    suite_test_path(stage_cleanup_path,sizeof(stage_cleanup_path),suite_fixture_directory,stage_name);
    free(stage_name);
    if(!mkdtemp(stage_cleanup_path)) fail("mkdtemp game fixture: %s",strerror(errno));
    char *stage=strdup(stage_cleanup_path);
    REQUIRE(stage,"Out of memory while creating game fixture");
    return stage;
}
static bool run_loader_case(const char *name,const char *kind,const char *game,const char *build,
                            const char *native,const char *rom,const char *output,
                            const char *result_path,unsigned opens,unsigned allocations,
                            bool truncated,unsigned objects,bool success,unsigned expected_words,
                            const int *search_results) {
    char *stage=create_stage(name);
    copy_stage(game,stage,false,!strcmp(kind,"missing-object"));
    copy_stage(build,stage,true,false);
    if(!strcmp(kind,"missing-object")) { char *object=join(stage,"OBJECT"); REQUIRE(!exists(object),"Missing-OBJECT fixture still contains the object file"); free(object); }
    char *words=join(stage,"WORDS.TOK");REQUIRE(exists(words),"Private KQ1 WORDS.TOK copy is missing");
    if(!strcmp(kind,"missing-words")) REQUIRE(!unlink(words),"Could not remove staged WORDS.TOK");
    else if(!strcmp(kind,"capacity")) write_overcapacity_words(words);
    else if(!strcmp(kind,"short-header")||!strcmp(kind,"excess-prefix")||!strcmp(kind,"header-offset")||
            !strcmp(kind,"header-order")||!strcmp(kind,"truncated-synonym")||!strcmp(kind,"word-order")) write_malformed_words_fixture(words,kind);
    else if(!strcmp(kind,"empty")||!strcmp(kind,"single")||!strcmp(kind,"prefix")||!strcmp(kind,"large")) write_words_search_fixture(words,kind);
    else REQUIRE(exists(words),"Private KQ1 WORDS.TOK copy is missing");
    if(truncated) { char *marker=join(stage,"loader-staging-read.marker");write_file(marker,"",0);free(marker); }
    write_file(result_path,"",0);
    char *program=join(stage,"agi.cx16"),*emulator=join(native,"x16emu"),*log_name=format("loader-%s.log",name),*log=join(output,log_name);free(log_name);
    char *command[]={emulator,"-rom",(char*)rom,"-fsroot",(char*)stage,"-startin",(char*)stage,
                     "-prg",program,"-run","-zeroram","-warp","-sound","none","-echo",NULL};
    fprintf(stderr,"RUN real X16 emulator loader case: %s\n",name);run_from(command,log,90,stage);
    unsigned expected_passes=success||truncated?2u:1u;
    bool okay=validate_loader_result(name,result_path,strcmp(kind,"missing-object")!=0,opens,allocations,truncated,expected_passes,objects,success,expected_words,search_results,!strcmp(kind,"missing-object"));
    cleanup_stage();
    free(stage);free(words);free(program);free(emulator);free(log);
    return okay;
}
static void run_loader_test(const char *game,const char *build,const char *native,const char *rom,const char *output) {
    suite_test_paths_init("loader");
    if(atexit(cleanup_stage)) fail("Cannot register loader fixture cleanup");
    static const int empty_results[LOADER_SEARCH_COUNT]={-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const int single_results[LOADER_SEARCH_COUNT]={37,-1,-1,-1,-1,-1,-1,-1,-1};
    static const int prefix_results[LOADER_SEARCH_COUNT]={11,12,13,-1,-1,-1,-1,-1,-1};
    static const int large_results[LOADER_SEARCH_COUNT]={357,-1,-1,-1,0,256,512,-1,-1};
    char *result_path=join(output,"loader-result.txt");unsigned failed=0;
    if(!run_loader_case("missing-object","missing-object",game,build,native,rom,output,result_path,0,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("missing-words","missing-words",game,build,native,rom,output,result_path,1,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("staging-read","staging-read",game,build,native,rom,output,result_path,2,3,true,1,false,0,NULL))failed++;
    if(!run_loader_case("over-capacity","capacity",game,build,native,rom,output,result_path,1,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("short-header","short-header",game,build,native,rom,output,result_path,1,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("excess-prefix","excess-prefix",game,build,native,rom,output,result_path,1,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("header-offset","header-offset",game,build,native,rom,output,result_path,1,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("header-order","header-order",game,build,native,rom,output,result_path,1,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("truncated-synonym","truncated-synonym",game,build,native,rom,output,result_path,1,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("descending-full-word-order","word-order",game,build,native,rom,output,result_path,1,0,false,1,false,0,NULL))failed++;
    if(!run_loader_case("empty-dictionary","empty",game,build,native,rom,output,result_path,2,3,false,1,true,0,empty_results))failed++;
    if(!run_loader_case("single-word","single",game,build,native,rom,output,result_path,2,3,false,1,true,1,single_results))failed++;
    if(!run_loader_case("shared-prefixes","prefix",game,build,native,rom,output,result_path,2,3,false,1,true,4,prefix_results))failed++;
    if(!run_loader_case("513-words","large",game,build,native,rom,output,result_path,2,3,false,1,true,513,large_results))failed++;
    free(result_path);
    REQUIRE(!failed,"Real X16 WORDS startup regression failed");
}
static void test_game_staging(void) {
    suite_test_paths_init("game-staging");
    char *game=join(suite_fixture_directory,"original"),*stage=join(suite_fixture_directory,"staged");
    mkdirs(join(game,"meta")); mkdirs(stage);
    write_file(join(game,"VOL.0"),"volume",6);
    write_file(join(game,"meta/KQ3-SOUND-IDS.IDX"),"index",5);
    copy_stage(game,stage,false,false);
    REQUIRE(file_contains(join(stage,"meta/KQ3-SOUND-IDS.IDX"),"index"),"Game staging omitted production dependency metadata");
    char *cache=join(suite_fixture_directory,"cache"),*cached_stage=join(suite_fixture_directory,"cached-stage");
    mkdirs(join(cache,"extracted")); mkdirs(cached_stage);
    write_file(join(cache,"extracted/KQ3-VIEW-IDS.BIN"),"views",5);
    copy_stage(cache,cached_stage,false,false);
    REQUIRE(file_contains(join(cached_stage,"meta/kq3-view-ids.bin"),"views"),"Shared extractor output did not reach the production metadata path");
    puts("PASS game staging retains dependency metadata");
}
int main(int argc,char **argv) {
    if(argc==2&&!strcmp(argv[1],"--staging-test")) { test_game_staging(); return 0; }
    char *emulator=NULL,*rom=NULL,*build=NULL,*game=NULL,*output=NULL,*sdl=NULL,*symbol=NULL;unsigned long trigger=180000000;bool foreground_test=false,picture_test=false,combined_test=false,loader_test=false,startup_text_test=false,random_ram=false,intro_test=false,play=false;
    bool watch_opening=getenv("CX16AGI_WATCH_OPENING")&&!strcmp(getenv("CX16AGI_WATCH_OPENING"),"1");
    bool visible_startup=watch_opening||(getenv("CX16AGI_VISIBLE_STARTUP")&&!strcmp(getenv("CX16AGI_VISIBLE_STARTUP"),"1"));
    unsigned startup_room=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--help")) {puts("Usage: test_full_game_irq --emulator-source DIR --rom FILE --build DIR --game DIR [--output DIR] [--symbols FILE] [--sdl-prefix DIR] [--trigger-cycle N] [--foreground-test] [--combined-irq-test] [--picture-test] [--loader-test] [--startup-text-test] [--startup-room 1|7] [--random-ram] [--intro-test] [--play]");return 0;}
        if(!strcmp(argv[i],"--play")) { play=true;random_ram=true;continue;}
        if(!strcmp(argv[i],"--intro-test")) { intro_test=true;if(!startup_room) startup_room=1;continue;}
        if(!strcmp(argv[i],"--random-ram")) { random_ram=true;continue;}
        if(!strcmp(argv[i],"--foreground-test")) {foreground_test=true;continue;}
        if(!strcmp(argv[i],"--picture-test")) {picture_test=true;continue;}
        if(!strcmp(argv[i],"--combined-irq-test")) {combined_test=true;continue;}
        if(!strcmp(argv[i],"--loader-test")) {loader_test=true;continue;}
        if(!strcmp(argv[i],"--startup-text-test")) {startup_text_test=true;continue;}
        REQUIRE(i+1<argc,"Missing option argument");char *option=argv[i],*value=argv[++i];
        if(!strcmp(option,"--startup-room")) { char *end; unsigned long n=strtoul(value,&end,10); REQUIRE(*value&&!*end&&(n==1||n==7),"Startup room must be 1 (KQ1) or 7 (KQ3)"); startup_room=(unsigned)n; }
        else if(!strcmp(option,"--emulator-source"))emulator=value;else if(!strcmp(option,"--rom"))rom=value;else if(!strcmp(option,"--build"))build=value;else if(!strcmp(option,"--game"))game=value;else if(!strcmp(option,"--output"))output=value;else if(!strcmp(option,"--sdl-prefix"))sdl=value;else if(!strcmp(option,"--symbols"))symbol=value;
        else if(!strcmp(option,"--trigger-cycle")) {char *end;errno=0;trigger=strtoul(value,&end,10);REQUIRE(!errno&&*value&&!*end&&trigger<=UINT32_MAX,"Invalid trigger cycle");}else fail("Unknown option %s",option);
    }
    REQUIRE(!watch_opening||(intro_test&&startup_room==7),"Watch opening requires the KQ3 intro scenario");
    REQUIRE(!visible_startup||startup_room,"Visible startup requires a startup scenario");
    REQUIRE(!play||!(startup_room||foreground_test||picture_test||combined_test||loader_test),"Interactive play must run separately from test scenarios");
    REQUIRE(!startup_room||!(foreground_test||picture_test||combined_test||loader_test),"Startup must run separately from injected command scenarios");
    REQUIRE(emulator&&rom&&build&&game,"Require --emulator-source, --rom, --build and --game");
    if(sdl) REQUIRE(exists(sdl),"SDL prefix does not exist");
    emulator=realpath(emulator,NULL);rom=realpath(rom,NULL);build=realpath(build,NULL);game=realpath(game,NULL);REQUIRE(emulator&&rom&&build&&game,"Input path does not exist");
    if(!output) {const char *tmp=getenv("TMPDIR");output=format("%s/full-game-irq-XXXXXX",tmp?tmp:"/tmp");REQUIRE(mkdtemp(output),"Cannot create temporary output");}else mkdirs(output);
    output=realpath(output,NULL);REQUIRE(output,"Cannot resolve output");
    size_t source_len=strlen(emulator);
    REQUIRE(strncmp(output,emulator,source_len)||(output[source_len]&&output[source_len]!='/'),"Output must be outside emulator source tree");
    /* A fresh output prevents stale logs or previously patched emulator sources. */
    REQUIRE(!exists(join(output,"emulator")),"Output already contains an emulator; use a fresh directory");
    char *source=join(output,"emulator"),*stage=NULL;copy_tree(emulator,source);
    if(!loader_test) {
        const char *fixture_name=play?"play-game":startup_text_test?"startup-text-game":startup_room?"startup-game":combined_test?"combined-game":picture_test?"picture-game":"foreground-game";
        suite_test_paths_init(fixture_name);
        if(atexit(cleanup_stage)) fail("Cannot register game fixture cleanup");
        stage=create_stage("game");
        copy_stage(game,stage,false,false);copy_stage(build,stage,true,false);
    }
    if(!play) {
    const char *candidates[]={"final.lbl","agi.cx16.lbl","final.dbg","agi.cx16.dbg"};
    if(!symbol)for(size_t i=0;i<4;i++) {char *p=join(build,candidates[i]);if(exists(p)){symbol=p;break;}free(p);}
    REQUIRE(symbol,"Provide --symbols with private assembly symbols");load_symbols(join(build,"final.lbl"));load_symbols(join(build,"agi.cx16.lbl"));load_symbols(symbol);
    if(startup_room) {
        const char *required[]={"_b4IsInited","_b4LoadUnloadResources","_var","_b6Interpret","_b7PollKeyboard","_b11DrawPic","_b2Show_pic","b9UpdateObjects","_offsetOfXPos","_offsetOfYPos","_offsetOfYSize","_viewtab"};
        for(size_t r=0;r<sizeof(required)/sizeof(*required);r++)
            for(size_t i=0;i<NAME_COUNT;i++) if(!strcmp(names[i],required[r])&&!found[i]) fail("Missing startup symbol %s",required[r]);
    }
    char *main_c=join(source,"src/main.c"),*cpu=join(source,"src/cpu/fake6502.c"),*video=join(source,"src/video.c"),*i2c=join(source,"src/i2c.c"),*memory=join(source,"src/memory.c");
    char *injected=NULL;size_t size=0;FILE *mem=open_memstream(&injected,&size);REQUIRE(mem,"Out of memory");
    char *qcsv=quoted_c(join(output,"irq.csv")),*qcommands=quoted_c(join(output,"commands.csv")),*qforeground=quoted_c(join(output,"foreground.csv")),*qcombined=quoted_c(join(output,"combined.csv")),*qinputtrace=quoted_c(join(output,"input-trace.csv")),*qvideotrace=quoted_c(join(output,"video-frames.csv"));
    char *qpicture=quoted_c(join(output,"picture.csv")),*qpicture_irq=quoted_c(join(output,"picture-irq.vram")),*qpicture_reference=quoted_c(join(output,"picture-reference.vram")),*qblit=quoted_c(join(output,"agi-blit-failures.csv")),*qview_retry=quoted_c(join(output,"view-load-retry.csv"));
    char *qloader_words=quoted_c("WORDS.TOK"),*qloader_result=quoted_c(join(output,"loader-result.txt"));
    fputs("#include <sys/stat.h>\n#include <unistd.h>\n#include <signal.h>\n",mem);
    fprintf(mem,"#define SHAKE_SPRITE_UPDATE_RECORD_BYTES %u\n#define SHAKE_SPRITE_UPDATE_MAX_RECORDS %u\n#define SHAKE_SPRITE_TERMINATOR_BYTES %u\n#define SHAKE_SPRITE_BUFFER_SIZE %u\n",SPRITE_UPDATE_RECORD_BYTES,SPRITE_UPDATE_MAX_RECORDS,SPRITE_UPDATE_TERMINATOR_BYTES,SPRITE_UPDATED_BUFFER_SIZE);
    fprintf(mem,"#define SHAKE_CSV %s\n#define SHAKE_COMMANDS %s\n#define SHAKE_INPUT_TRACE %s\n#define SHAKE_VIDEO_CSV %s\n#define FOREGROUND_CSV %s\n#define FOREGROUND_TEST_ENABLED %u\n#define COMBINED_CSV %s\n#define COMBINED_IRQ_TEST_ENABLED %u\n#define PICTURE_CSV %s\n#define PICTURE_IRQ_VRAM %s\n#define PICTURE_REFERENCE_VRAM %s\n#define BLIT_FAILURE_CSV %s\n#define VIEW_LOAD_RETRY_CSV %s\n#define PICTURE_TEST_ENABLED %u\n#define LOADER_TEST_ENABLED %u\n#define STARTUP_TEXT_TEST_ENABLED %u\n#define LOADER_WORDS_PATH %s\n#define LOADER_RESULT_PATH %s\n#define TRIGGER_CYCLE %luu\n",qcsv,qcommands,qinputtrace,qvideotrace,qforeground,(foreground_test||picture_test)?1:0,qcombined,combined_test?1:0,qpicture,qpicture_irq,qpicture_reference,qblit,qview_retry,picture_test?1:0,loader_test?1:0,startup_text_test?1:0,qloader_words,qloader_result,trigger);
    free(qcsv);free(qcommands);free(qinputtrace);free(qvideotrace);free(qforeground);free(qcombined);free(qpicture);free(qpicture_irq);free(qpicture_reference);free(qblit);free(qview_retry);free(qloader_words);free(qloader_result);
    for(size_t i=0;i<NAME_COUNT;i++) {if(!found[i]&&!startup_room)fail("Missing symbol %s",names[i]);if(names[i][0]=='@')fprintf(mem,"#define S_local_%s 0x%x\n",names[i]+1,labels[i]);else fprintf(mem,"#define S_%s 0x%x\n",names[i],labels[i]);}
    for(size_t i=0;i<ALLOCATOR_NAME_COUNT;i++) {if(!allocator_found[i]&&!startup_room)fail("Missing allocator symbol %s",allocator_names[i]);fprintf(mem,"#define S_%s 0x%x\n",allocator_names[i],allocator_labels[i]);}
    for(size_t i=0;i<LOADER_NAME_COUNT;i++) {
        if(loader_test&&!loader_found[i]) fail("Missing loader symbol %s",loader_names[i]);
        fprintf(mem,"#define S_%s 0x%x\n",loader_names[i],loader_found[i]?loader_labels[i]:0);
    }
    for(size_t i=0;i<LOADER_SEARCH_NAME_COUNT;i++) {
        if(loader_test&&!loader_search_found[i]) fail("Missing WORDS search target symbol %s",loader_search_names[i]);
        fprintf(mem,"#define S_%s 0x%x\n",loader_search_names[i],loader_search_found[i]?loader_search_labels[i]:0);
    }
    for(size_t i=0;i<STARTUP_NAME_COUNT;i++) {
        if(startup_text_test&&!startup_found[i]) fail("Missing startup trace symbol %s",startup_names[i]);
        fprintf(mem,"#define S_%s 0x%x\n",startup_names[i],startup_found[i]?startup_labels[i]:0);
    }
    for(size_t i=0;i<FIXTURE_NAME_COUNT;i++) {
        if(picture_test&&!fixture_found[i]) fail("Missing sprite fixture target symbol %s",fixture_names[i]);
        fprintf(mem,"#define S_%s 0x%x\n",fixture_names[i],fixture_found[i]?fixture_labels[i]:0);
    }
    fprintf(mem,"#define WATCH_OPENING_ENABLED %u\n",watch_opening?1:0);
    fprintf(mem,"#define STARTUP_TEST_ROOM %u\n#define INTRO_TEST_ENABLED %u\nstatic void startup_before_step(void);\n",startup_room,intro_test?1:0);
    fputs(hook,mem);fputs(startup_hook,mem);fputs("\nvoid *emulator_loop(void *param);",mem);REQUIRE(!fclose(mem),"Hook buffer close failed");patch(main_c,"void *emulator_loop(void *param);",injected,1);free(injected);
    patch(main_c,"\t\tstep6502();","\t\tshake_before_step();\n\t\tuint8_t shake_opcode=waiting ? 0 : debug_read6502(regs.pc,regs.k,USE_CURRENT_X16_BANK);\n\t\tstep6502();\n\t\tshake_after_step(shake_opcode);",1);
    patch(cpu,"void irq6502() {","extern void shake_irq_accepted(void);\nextern int shake_suppress_irq(void);\nvoid irq6502() {",1);
    patch(cpu,"        interrupt6502(INT_IRQ);","        if(!shake_suppress_irq()) { shake_irq_accepted(); interrupt6502(INT_IRQ); }",1);
    patch(memory,"read6502(uint16_t address, uint8_t bank) {\n\tif (!is_gen2) bank = 0;","read6502(uint16_t address, uint8_t bank) {\n\textern void shake_text_source_read(uint16_t,uint8_t,uint16_t,uint8_t);\n\tshake_text_source_read(address,bank,opcode_addr,regs.k);\n\tif (!is_gen2) bank = 0;",1);
    if(watch_opening) patch(video,"while (SDL_PollEvent(&event)) {","extern bool cx16agiOpeningReady;\n\twhile (SDL_PollEvent(&event)) {\n\t\tif(!cx16agiOpeningReady && (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP)) continue;",1);
    patch(video,"void video_write(uint8_t reg, uint8_t value) {","void shake_get_display(uint8_t *values) { for(int i=0;i<4;i++) values[i]=reg_composer[4+i]; values[4]=reg_composer[3]; }\nextern void shake_window_write(void);\nextern void shake_foreground_vera_register_write(uint8_t,uint8_t,uint8_t,uint8_t,uint16_t,uint8_t,uint8_t);\nvoid video_write(uint8_t reg, uint8_t value) {\n\tif(reg<=2 || reg==5) { uint8_t oldvalue=0; if(reg==0) oldvalue=io_addr[io_addrsel]&0xff; else if(reg==1) oldvalue=(io_addr[io_addrsel]>>8)&0xff; else if(reg==2) oldvalue=((io_addr[io_addrsel]>>16)&1)|(fx_nibble_bit[io_addrsel]<<1)|(fx_nibble_incr[io_addrsel]<<2)|(io_inc[io_addrsel]<<3); else oldvalue=(io_dcsel<<1)|io_addrsel; shake_foreground_vera_register_write(reg,io_dcsel,value,oldvalue,opcode_addr,regs.k,memory_get_ram_bank()); }\n\tif(io_dcsel == 1 && reg>=9 && reg<=12) shake_window_write();",1);
    patch(video,"\t\tisr |= 1; // VSYNC IRQ","\t\textern void shake_vsync_asserted(void);\n\t\tshake_vsync_asserted();\n\t\tisr |= 1; // VSYNC IRQ",1);
    patch(video,"video_space_write(uint32_t address, uint8_t value)\n{","video_space_write(uint32_t address, uint8_t value)\n{\n\textern void shake_sprite_write(uint32_t,uint8_t);\n\textern void shake_foreground_vram_write(uint32_t);\n\tshake_foreground_vram_write(address);\n\tif(address>=0x1fc00 && address<0x20000) shake_sprite_write(address,value);",1);
    patch(video,"\tif(address>=0x1fc00 && address<0x20000) shake_sprite_write(address,value);\n\tvideo_ram[address & 0x1FFFF] = value;","\tif(address>=0x1fc00 && address<0x20000) shake_sprite_write(address,value);\n\tvideo_ram[address & 0x1FFFF] = value;\n\t{ extern void shake_combined_vram_write(uint32_t,uint8_t); shake_combined_vram_write(address,value); }",1);
    patch(video,"fx_video_space_write(uint32_t address, bool nibble, uint8_t value)\n{","fx_video_space_write(uint32_t address, bool nibble, uint8_t value)\n{\n\textern void shake_sprite_write(uint32_t,uint8_t);\n\textern void shake_foreground_vram_write(uint32_t);\n\tshake_foreground_vram_write(address);\n\tif(address>=0x1fc00 && address<0x20000) shake_sprite_write(address,value);",1);
    patch(video,"if (!fx_trans_writes || value > 0) video_ram[address & 0x1FFFF] = value;","if (!fx_trans_writes || value > 0) { video_ram[address & 0x1FFFF] = value; extern void shake_combined_vram_write(uint32_t,uint8_t); shake_combined_vram_write(address,value); }",1);
    patch(video,"\t\t\tcase 3:\n\t\t\t\t// Do nothing\n\t\t\t\tbreak;\n\t\t}\n\t}\n}","\t\t\tcase 3:\n\t\t\t\t// Do nothing\n\t\t\t\tbreak;\n\t\t}\n\t\textern void shake_combined_vram_write(uint32_t,uint8_t);\n\t\tshake_combined_vram_write(address,video_ram[address & 0x1FFFF]);\n\t}\n}",1);
    patch(video,"psg_writereg(address & 0x3f, value);","extern void shake_psg_write(uint8_t reg,uint8_t value);\n\t\tshake_psg_write(address & 0x3f,value);\n\t\tpsg_writereg(address & 0x3f, value);",2);
    patch(video,"#include \"video.h\"","#include \"video.h\"\n#include \"cpu/fake6502.h\"",1);
    patch(video,"\t\tif (record_gif == RECORD_GIF_SINGLE) {","\t\tif (record_gif > RECORD_GIF_PAUSED) {\n\t\t\textern void shake_video_frame_written(uint32_t,uint32_t,uint16_t,uint8_t);\n\t\t\tshake_video_frame_written((uint32_t)frame_count, clockticks6502, regs.pc, memory_get_ram_bank());\n\t\t}\n\t\tif (record_gif == RECORD_GIF_SINGLE) {",1);
    patch(i2c,"\nvoid i2c_reset_state() {","\nvoid shake_get_i2c_state(uint8_t *out) { out[0]=i2c_port.clk_in; out[1]=i2c_port.data_in; out[2]=i2c_port.data_out; out[3]=(uint8_t)(state+1); out[4]=read_mode; out[5]=value; out[6]=(uint8_t)count; out[7]=device; }\n\nvoid i2c_reset_state() {",1);
    }
    if(play||visible_startup) {
        const char *adapter=getenv("CX16AGI_TEST_AUDIO_ADAPTER");
        REQUIRE(adapter&&exists(adapter),"Interactive scenarios require the suite audio adapter");
        char *upstream_audio=join(source,"src/audio-upstream.c");
        copy_file(join(source,"src/audio.c"),upstream_audio);
        copy_file(adapter,join(source,"src/audio.c"));
    }
    patch(join(source,"src/disasm.c"),"char *where = strstr(mnemonic, \"%%0%hhux\");","const char *where = strstr(mnemonic, \"%%0%hhux\");",1);
    char *native=join(output,"native"),*buildlog=join(output,"build.log");
    if(!sdl) {char *parent=strdup(emulator),*slash=strrchr(parent,'/');REQUIRE(slash,"Invalid emulator path");*slash=0;sdl=join(parent,"sdl2-install");free(parent);}
    char *prefix=NULL;if(exists(sdl)) {char *absolute=realpath(sdl,NULL);REQUIRE(absolute,"Cannot resolve SDL prefix");prefix=format("-DCMAKE_PREFIX_PATH=%s",absolute);free(absolute);}
    char *configure[]={"cmake","-S",source,"-B",native,"-DCMAKE_BUILD_TYPE=Release",(play||visible_startup)?"-DENABLE_LTO=ON":"-DENABLE_LTO=OFF","-DENABLE_TRACE=OFF","-DENABLE_FLUIDSYNTH=OFF",prefix,NULL};run_from(configure,buildlog,0,NULL);
    char *compile[]={"cmake","--build",native,"-j4",NULL};run_from(compile,buildlog,0,NULL);
    if(!play&&!visible_startup) REQUIRE(!setenv("SDL_VIDEODRIVER","dummy",1)&&!setenv("SDL_AUDIODRIVER","dummy",1),"Cannot set SDL environment");
    if(loader_test) { run_loader_test(game,build,native,rom,output); return 0; }
    if(startup_text_test) {
        char *command[24];int command_length=0;
        command[command_length++]=join(native,"x16emu");command[command_length++]="-rom";command[command_length++]=rom;
        command[command_length++]="-fsroot";command[command_length++]=stage;command[command_length++]="-startin";command[command_length++]=stage;
        command[command_length++]="-prg";command[command_length++]=join(stage,"agi.cx16");command[command_length++]="-run";command[command_length++]="-zeroram";
        command[command_length++]="-warp";command[command_length++]="-sound";command[command_length++]="none";
        command[command_length++]="-gif";command[command_length++]=format("%s/game.gif",output);
        command[command_length++]="-echo";command[command_length]=NULL;
        run(command,join(output,"run.log"),300);
    } else {
        char *command[26];int command_length=0;
        command[command_length++]=join(native,"x16emu");command[command_length++]="-rom";command[command_length++]=rom;
        command[command_length++]="-fsroot";command[command_length++]=stage;command[command_length++]="-startin";command[command_length++]=stage;
        command[command_length++]="-prg";command[command_length++]=join(stage,"agi.cx16");command[command_length++]="-run";if(!random_ram) command[command_length++]="-zeroram";
        if(visible_startup) { command[command_length++]="-log";command[command_length++]="K"; }
        if(!play&&!visible_startup) { command[command_length++]="-warp";command[command_length++]="-sound";command[command_length++]="none"; }
        if(!play&&!visible_startup&&!startup_room&&!picture_test&&!combined_test) { command[command_length++]="-gif";command[command_length++]=format("%s/game.gif,wait",output); }
        command[command_length++]="-echo";command[command_length]=NULL;
        run(command,join(output,"run.log"),(play||watch_opening)?0:300);
    }
    cleanup_stage();
    if(!stage||exists(stage)) fail("Integration fixture cleanup regression: private staged game was not removed");
    require_no_retained_stage(output);
    if(startup_text_test) {
        char *log=read_file(join(output,"run.log"),NULL);
        const char *summary_needle="Production KQ1 startup text passed: 23 Logic83 phases, 221 display.v returns, 13-row cap, blank rows, frame glyphs and RGB verified";
        const char *summary=strstr(log,summary_needle);
        REQUIRE(summary,"Natural KQ1 Logic83 startup text sequence did not complete");
        REQUIRE(strstr(log,"RGB VSYNC samples="),"Natural KQ1 startup text did not report VSYNC RGB samples");
        const char *injected=strstr(log,"KQ1 startup glyph negative control injected:");
        const char *rejected=strstr(log,"KQ1 startup glyph negative control rejected:");
        const char *restored=strstr(log,"KQ1 startup glyph negative control restored:");
        REQUIRE(injected,"Transient published-glyph corruption was not sampled");
        REQUIRE(rejected,"VSYNC observer did not reject the transient published-glyph corruption");
        REQUIRE(restored,"Restored published glyph did not pass the frame observer");
        const char *records[]={summary,injected,rejected,restored};
        for(size_t i=0;i<sizeof(records)/sizeof(records[0]);i++) {
            const char *end=strchr(records[i],'\n');
            size_t length=end?(size_t)(end-records[i]):strlen(records[i]);
            fprintf(stderr,"%.*s\n",(int)length,records[i]);
        }
        free(log);
        char *gif=join(output,"game.gif");
        size_t gifsize;
        unsigned char *header=(unsigned char*)read_file(gif,&gifsize);
        REQUIRE(gifsize>40&&(!memcmp(header,"GIF87a",6)||!memcmp(header,"GIF89a",6))&&header[gifsize-1]==0x3B,
                "Startup text GIF capture is incomplete");
        free(header);
        free(gif);
    }
    else if(play) { puts("Interactive emulator closed; staged game cleaned"); return 0; }
    else {
        if(intro_test&&startup_room==1) {
            char *log=read_file(join(output,"run.log"),NULL);
            char *row=strstr(log,"Natural intro credit batch:");
            REQUIRE(row,"Missing intro workload evidence");
            char *end=strchr(row,'\n'); if(end) *end=0;
            puts(row); free(log);
        }
        if(startup_room) {
            REQUIRE(file_contains(join(output,"run.log"),intro_test?"PASS natural intro presentation":startup_room==7?"PASS natural game startup and courtyard round trip colours":"PASS natural game startup"),"Game did not reach its playable room");
            puts(intro_test?(startup_room==7?"PASS natural KQ3 titles and story opening without input":"PASS natural intro text publication and audio cadence"):"PASS natural game startup and sustained interpreter/input/render progress");
        } else if(picture_test) validate_picture(output);
        else if(combined_test) validate_combined(output);
        else validate(output,foreground_test);
    }
    return 0;

}
