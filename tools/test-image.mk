# Fresh production image and private debug symbols for the native C tests.
# Invoke from any directory: make -f /path/to/tools/test-image.mk OUT=/path/to/build
REPO_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OUT ?= $(REPO_ROOT)/build/test-image
ifdef CC65_HOME
CC65 := $(CC65_HOME)/bin/cc65
CL65 := $(CC65_HOME)/bin/cl65
CA65 := $(CC65_HOME)/bin/ca65
else
CC65 := cc65
CL65 := cl65
CA65 := ca65
endif

# Preserve the production optimizer exclusions. OptStackOps is omitted because
# current cc65 rejects that optimizer name; OptStackPtrOps is supported.
TEST_CC65_CFLAGS := -O $(foreach opt,OptPush1 OptPush2 OptPushPop1 OptStore1 OptStore2 OptStore3 OptStore4 OptStore5 OptStackPtrOps OptGotoSPAdj OptUnusedStores OptStoreLoad,--disable-opt $(opt))

.PHONY: all
all:
	@set -eu; \
	repo='$(REPO_ROOT)'; out='$(OUT)'; \
	mkdir -p "$$out/obj"; \
	set --; \
	for source in "$$repo"/src/*.c "$$repo/tools/test_banked_strings_target.c" "$$repo/tools/test_banked_strings_bounded_target.c" "$$repo/tools/test_banked_strings_input_target.c" "$$repo/tools/test_banked_strings_metadata_target.c" "$$repo/tools/test_banked_strings_allocator_target.c" "$$repo/tools/test_banked_strings_commands_target.c" "$$repo/tools/test_banked_strings_sound_target.c" "$$repo/tools/test_object_commands_target.c" "$$repo/tools/test_show_obj_allocation_target.c" "$$repo/tools/test_timer_target.c"; do \
	  name=$${source##*/}; stem=$${name%.c}; object="$$out/obj/$$stem.o"; assembly="$$out/obj/$$stem.s"; \
	  echo "CC $$name"; \
	  '$(CC65)' -t cx16 -I "$$repo/src" $(TEST_CC65_CFLAGS) -o "$$assembly" "$$source"; \
	  '$(CA65)' -t cx16 -I "$$repo/src" -o "$$object" "$$assembly"; \
	  set -- "$$@" "$$object"; \
	done; \
	'$(CA65)' -g -t cx16 -I "$$repo/src" "$$repo/tools/test_image_exports.s" -o "$$out/obj/main-asm.o"; \
	'$(CL65)' -t cx16 -C "$$repo/cx16-bank.cfg" -o "$$out/agi.cx16" \
	  -Ln "$$out/final.lbl" -m "$$out/final.map" -Wl "--dbgfile,$$out/final.dbg" \
	  "$$@" "$$out/obj/main-asm.o"
