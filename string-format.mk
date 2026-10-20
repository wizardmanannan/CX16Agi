HOSTCC ?= cc
CX16AGI_BUILD_DIR ?= $(HOME)/.cache/cx16agi/build
STRING_FORMAT_OUT := $(CX16AGI_BUILD_DIR)/obj/host/string-format
STRING_FORMAT_CYCLE_BUDGET ?= 86585
STRING_FORMAT_FLAGS := -std=c99 -D_MSC_VER -Itests/stubs -Isrc -include tests/stubs/native.h -ffunction-sections -fdata-sections -Wno-unknown-pragmas
CC65_HOME ?= $(HOME)/.cache/cx16agi/tools/cc65
X16EMU ?= $(HOME)/.cache/cx16agi/tools/x16-emulator/build-tas/x16emu
X16_ROM ?= $(HOME)/.cache/cx16agi/tools/rom/rom.bin
CC65 ?= $(if $(CC65_HOME),$(CC65_HOME)/bin/cc65,cc65)
CA65 ?= $(if $(CC65_HOME),$(CC65_HOME)/bin/ca65,ca65)

.PHONY: string-format-test
string-format-test:
	@mkdir -p $(STRING_FORMAT_OUT)
	@{ flock -n 9 || { echo 'Build output busy; set CX16AGI_BUILD_DIR to a private base.' >&2; exit 1; }; \
	ulimit -c 0; \
	CC65_HOME='$(CC65_HOME)' X16EMU='$(X16EMU)' X16_ROM='$(X16_ROM)' bash tests/run-string-format-target.sh '$(STRING_FORMAT_OUT)' '$(CURDIR)' '$(STRING_FORMAT_CYCLE_BUDGET)' '$(HOSTCC)' && \
	$(HOSTCC) $(STRING_FORMAT_FLAGS) tests/test-string-commands.c src/commands.c -Wl,--gc-sections -o $(STRING_FORMAT_OUT)/test-string-commands && \
	$(STRING_FORMAT_OUT)/test-string-commands && \
	$(HOSTCC) $(STRING_FORMAT_FLAGS) tests/test-menu-text.c src/menu.c -Wl,--gc-sections -o $(STRING_FORMAT_OUT)/test-menu-text && \
	$(STRING_FORMAT_OUT)/test-menu-text; } 9>$(CX16AGI_BUILD_DIR)/.string-format.lock

.PHONY: string-format-target-check
string-format-target-check:
	@mkdir -p $(STRING_FORMAT_OUT)
	@{ flock -n 9 || { echo 'Build output busy; set CX16AGI_BUILD_DIR to a private base.' >&2; exit 1; }; \
	set -e; for name in stringFormat parser textLayer commands menu; do \
	$(CC65) -t cx16 -O -o $(STRING_FORMAT_OUT)/$$name.s src/$$name.c; \
	$(CA65) -t cx16 -o $(STRING_FORMAT_OUT)/$$name.o $(STRING_FORMAT_OUT)/$$name.s; \
	done; } 9>$(CX16AGI_BUILD_DIR)/.string-format.lock
