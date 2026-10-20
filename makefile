###############################################################################
### Generic Makefile for cc65 projects - full version with abstract options ###
### V1.3.0(w) 2010 - 2013 Oliver Schmidt & Patryk "Silver Dream !" ?ogiewa  ###
###############################################################################

###############################################################################
### In order to override defaults - values can be assigned to the variables ###
###############################################################################

# Space or comma separated list of cc65 supported target platforms to build for.
# Default: c64 (lowercase!)
TARGETS := cx16

# Name of the final, single-file executable.
# Default: name of the current dir with target name appended
PROGRAM := agi

# Path(s) to additional libraries required for linking the program
# Use only if you don't want to place copies of the libraries in SRCDIR
# Default: none
LIBS    :=

# Custom linker configuration file
# Use only if you don't want to place it in SRCDIR
# Default: none
CONFIG  :=

# Additional C compiler flags and options.
# Default: none
# OptStackOps was removed in cc65 2.19; keep the remaining project-specific
# exclusions because changing these has broken the interpreter's 3D rendering.
CFLAGS  = -O -Wc --disable-opt,OptPush1 -Wc --disable-opt,OptPush2 -Wc --disable-opt,OptPushPop1 -Wc --disable-opt,OptStore1 -Wc --disable-opt,OptStore2 -Wc --disable-opt,OptStore3 -Wc --disable-opt,OptStore4 -Wc --disable-opt,OptStore5 -Wc --disable-opt,OptStackPtrOps -Wc --disable-opt,OptGotoSPAdj -Wc --disable-opt,OptUnusedStores -Wc --disable-opt,OptStoreLoad

# Additional assembler flags and options.
# Default: none
ASFLAGS =

# Additional linker flags and options.
# Default: none
LDFLAGS =

# Path to the directory containing C and ASM sources.
# Default: src
SRCDIR :=

# Path to the directory where object files are to be stored (inside respective target subdirectories).
# Default: obj
OBJDIR :=

# Command used to run the emulator.
# Default: depending on target platform. For default (c64) target: x64 -kernal kernal -VICIIdsize -autoload
EMUCMD :=

# Optional commands used before starting the emulation process, and after finishing it.
# Default: none
# Examples
#PREEMUCMD := osascript -e "tell application \"System Events\" to set isRunning to (name of processes) contains \"X11.bin\"" -e "if isRunning is true then tell application \"X11\" to activate"
#PREEMUCMD := osascript -e "tell application \"X11\" to activate"
#POSTEMUCMD := osascript -e "tell application \"System Events\" to tell process \"X11\" to set visible to false"
#POSTEMUCMD := osascript -e "tell application \"Terminal\" to activate"
PREEMUCMD :=
POSTEMUCMD :=

# On Windows machines VICE emulators may not be available in the PATH by default.
# In such case, please set the variable below to point to directory containing
# VICE emulators. 
#VICE_HOME := "C:\Program Files\WinVICE-2.2-x86\"
VICE_HOME :=

# Options state file name. You should not need to change this, but for those
# rare cases when you feel you really need to name it differently - here you are
STATEFILE := Makefile.options

###################################################################################
####  DO NOT EDIT BELOW THIS LINE, UNLESS YOU REALLY KNOW WHAT YOU ARE DOING!  ####
###################################################################################

###################################################################################
### Mapping abstract options to the actual compiler, assembler and linker flags ###
### Predefined compiler, assembler and linker flags, used with abstract options ###
### valid for 2.14.x. Consult the documentation of your cc65 version before use ###
###################################################################################

# Compiler flags used to tell the compiler to optimise for SPEED
define _optspeed_
  CFLAGS += -Oris
endef

# Compiler flags used to tell the compiler to optimise for SIZE
define _optsize_
  CFLAGS += -Or
endef

# Compiler and assembler flags for generating listings
define _listing_
  CFLAGS += --listing $$(@:.o=.lst)
  ASFLAGS += --listing $$(@:.o=.lst)
  REMOVES += $(addsuffix .lst,$(basename $(OBJECTS)))
endef

# Linker flags for generating map file
define _mapfile_
  LDFLAGS += --mapfile $$@.map
  REMOVES += $(PROGRAM).map
endef

# Linker flags for generating VICE label file
define _labelfile_
  LDFLAGS += -Ln $$@.lbl
  REMOVES += $(PROGRAM).lbl
endef

# Linker flags for generating a debug file
define _debugfile_
  LDFLAGS += -Wl --dbgfile,$$@.dbg
  REMOVES += $(PROGRAM).dbg
endef

###############################################################################
###  Defaults to be used if nothing defined in the editable sections above  ###
###############################################################################

# Presume the C64 target like the cl65 compile & link utility does.
# Set TARGETS to override.
ifeq ($(TARGETS),)
  TARGETS := c64
endif

# Presume we're in a project directory so name the program like the current
# directory. Set PROGRAM to override.
ifeq ($(PROGRAM),)
  PROGRAM := $(notdir $(CURDIR))
endif

# Presume the C and asm source files to be located in the subdirectory 'src'.
# Set SRCDIR to override.
ifeq ($(SRCDIR),)
  SRCDIR := src
endif

# Presume the object and dependency files to be located in the subdirectory
# 'obj' (which will be created). Set OBJDIR to override.
ifeq ($(OBJDIR),)
  OBJDIR := obj
endif
TARGETOBJDIR := $(OBJDIR)/$(TARGETS)

# On Windows it is mandatory to have CC65_HOME set. So do not unnecessarily
# rely on cl65 being added to the PATH in this scenario.
ifdef CC65_HOME
  CC := $(CC65_HOME)/bin/cl65
else
  CC := cl65
endif

# Default emulator commands and options for particular targets.
# Set EMUCMD to override.
c64_EMUCMD := $(VICE_HOME)x64 -kernal kernal -VICIIdsize -autoload
c128_EMUCMD := $(VICE_HOME)x128 -kernal kernal -VICIIdsize -autoload
vic20_EMUCMD := $(VICE_HOME)xvic -kernal kernal -VICdsize -autoload
pet_EMUCMD := $(VICE_HOME)xpet -Crtcdsize -autoload
plus4_EMUCMD := $(VICE_HOME)xplus4 -TEDdsize -autoload
# So far there is no x16 emulator in VICE (why??) so we have to use xplus4 with -memsize option
c16_EMUCMD := echo Yes #$(VICE_HOME)xplus4 -ramsize 16 -TEDdsize -autoload
cbm510_EMUCMD := $(VICE_HOME)xcbm2 -model 510 -VICIIdsize -autoload
cbm610_EMUCMD := $(VICE_HOME)xcbm2 -model 610 -Crtcdsize -autoload
atari_EMUCMD := atari800 -windowed -xl -pal -nopatchall -run

ifeq ($(EMUCMD),)
  EMUCMD = $($(CC65TARGET)_EMUCMD)
endif

###############################################################################
### The magic begins                                                        ###
###############################################################################

# The "Native Win32" GNU Make contains quite some workarounds to get along with
# cmd.exe as shell. However it does not provide means to determine that it does
# actually activate those workarounds. Especially $(SHELL) does NOT contain the
# value 'cmd.exe'. So the usual way to determine if cmd.exe is being used is to
# execute the command 'echo' without any parameters. Only cmd.exe will return a
# non-empty string - saying 'ECHO is on/off'.
#
# Many "Native Win32" programs accept '/' as directory delimiter just fine. How-
# ever the internal commands of cmd.exe generally require '\' to be used.
#
# cmd.exe has an internal command 'mkdir' that doesn't understand nor require a
# '-p' to create parent directories as needed.
#
# cmd.exe has an internal command 'del' that reports a syntax error if executed
# without any file so make sure to call it only if there's an actual argument.
ifeq ($(shell echo),)
  MKDIR = mkdir -p $1
  RMDIR = rmdir $1
  RMFILES = $(RM) $1
else
  MKDIR = mkdir $(subst /,\,$1)
  RMDIR = rmdir $(subst /,\,$1)
  RMFILES = $(if $1,del /f $(subst /,\,$1))
endif
COMMA := ,
SPACE := $(N/A) $(N/A)
define NEWLINE


endef
# Note: Do not remove any of the two empty lines above !

TARGETLIST := $(subst $(COMMA),$(SPACE),$(TARGETS))

ifeq ($(words $(TARGETLIST)),1)

# Set PROGRAM to something like 'myprog.c64'.
override PROGRAM := $(PROGRAM).$(TARGETLIST)

# Set SOURCES to something like 'src/foo.c src/bar.s'.
# Use of assembler files with names ending differently than .s is deprecated!
SOURCES := $(wildcard $(SRCDIR)/*.c)
SOURCES += $(wildcard $(SRCDIR)/*.C)
SOURCES += $(SRCDIR)/main.s
SOURCES += $(wildcard $(SRCDIR)/*.a65)

# Add to SOURCES something like 'src/c64/me.c src/c64/too.s'.
# Use of assembler files with names ending differently than .s is deprecated!
SOURCES += $(wildcard $(SRCDIR)/$(TARGETLIST)/*.c)
SOURCES += $(SRCDIR)/main.s
SOURCES += $(wildcard $(SRCDIR)/$(TARGETLIST)/*.a65)

# Set OBJECTS to something like 'obj/c64/foo.o obj/c64/bar.o'.
OBJECTS := $(addsuffix .o,$(basename $(addprefix $(TARGETOBJDIR)/,$(notdir $(SOURCES)))))

# Set DEPENDS to something like 'obj/c64/foo.d obj/c64/bar.d'.
DEPENDS := $(OBJECTS:.o=.d)

# Add to LIBS something like 'src/foo.lib src/c64/bar.lib'.
LIBS += $(wildcard $(SRCDIR)/*.lib)
LIBS += $(wildcard $(SRCDIR)/$(TARGETLIST)/*.lib)

# Add to CONFIG something like 'src/c64/bar.cfg src/foo.cfg'.
CONFIG += $(wildcard $(SRCDIR)/$(TARGETLIST)/*.cfg)
CONFIG += $(wildcard $(SRCDIR)/*.cfg)

# Select CONFIG file to use. Target specific configs have higher priority.
ifneq ($(word 2,$(CONFIG)),)
  CONFIG := $(firstword $(CONFIG))
  $(info Using config file $(CONFIG) for linking)
endif

.SUFFIXES:
.PHONY: all test clean zap love

all: $(PROGRAM)

-include $(DEPENDS)
-include $(STATEFILE)

# If OPTIONS are given on the command line then save them to STATEFILE
# if (and only if) they have actually changed. But if OPTIONS are not
# given on the command line then load them from STATEFILE. Have object
# files depend on STATEFILE only if it actually exists.
ifeq ($(origin OPTIONS),command line)
  ifneq ($(OPTIONS),$(_OPTIONS_))
    ifeq ($(OPTIONS),)
      $(info Removing OPTIONS)
      $(shell $(RM) $(STATEFILE))
      $(eval $(STATEFILE):)
    else
      $(info Saving OPTIONS=$(OPTIONS))
      $(shell echo _OPTIONS_=$(OPTIONS) > $(STATEFILE))
    endif
    $(eval $(OBJECTS): $(STATEFILE))
  endif
else
  ifeq ($(origin _OPTIONS_),file)
    $(info Using saved OPTIONS=$(_OPTIONS_))
    OPTIONS = $(_OPTIONS_)
    $(eval $(OBJECTS): $(STATEFILE))
  endif
endif

# Transform the abstract OPTIONS to the actual cc65 options.
$(foreach o,$(subst $(COMMA),$(SPACE),$(OPTIONS)),$(eval $(_$o_)))

# Strip potential variant suffix from the actual cc65 target.
CC65TARGET := $(firstword $(subst .,$(SPACE),$(TARGETLIST)))

# The remaining targets.
$(TARGETOBJDIR):
	$(call MKDIR,$@)

vpath %.c $(SRCDIR)/$(TARGETLIST) $(SRCDIR)

$(TARGETOBJDIR)/%.o: %.c | $(TARGETOBJDIR)
	$(CC) -t $(CC65TARGET) -c --create-dep $(@:.o=.d) $(CFLAGS) -o $@ $<

vpath %.s $(SRCDIR)/$(TARGETLIST) $(SRCDIR)

$(TARGETOBJDIR)/%.o: %.s | $(TARGETOBJDIR)
	$(CC) -t $(CC65TARGET) -c --create-dep $(@:.o=.d) $(ASFLAGS) -o $@ $<

vpath %.asm $(SRCDIR)/$(TARGETLIST) $(SRCDIR)

$(TARGETOBJDIR)/%.o: %.asm | $(TARGETOBJDIR)
	$(CC) -t $(CC65TARGET) -c --create-dep $(@:.o=.d) $(ASFLAGS) -o $@ $<

vpath %.a65 $(SRCDIR)/$(TARGETLIST) $(SRCDIR)

$(TARGETOBJDIR)/%.o: %.a65 | $(TARGETOBJDIR)
	$(CC) -O -t $(CC65TARGET) -c --create-dep $(@:.o=.d) $(ASFLAGS) -o $@ $<

$(PROGRAM): $(CONFIG) $(OBJECTS) $(LIBS)
	$(CC) -t $(CC65TARGET) $(LDFLAGS) -o $@ $(patsubst %.cfg,-C %.cfg,$^) -C cx16-bank.cfg --mapfile $@.map

HOSTCC ?= cc
X16_EMULATOR_REVISION ?= 77f2bab332e24c5456f9844a6c7ddd203f6198e6
X16_EMULATOR_SOURCE_DIR ?= $(TARGETOBJDIR)/x16-emulator-source
X16_TEST_EMULATOR := $(X16_EMULATOR_SOURCE_DIR)/build/x16emu
X16EMU ?= $(X16_TEST_EMULATOR)
X16ROM ?= $(if $(CC65_HOME),$(abspath $(dir $(CC65_HOME))/../rom-r49/rom.bin),rom.bin)
CX16_GAMES_DIR ?= /home/arbitrator/.cache/cx16agi/tools/games
TEST_DISPATCH := $(TARGETOBJDIR)/test-logic-dispatch
TEST_DISPATCH_DIRECT := $(TARGETOBJDIR)/test-logic-dispatch-direct
TEST_DISPATCH_BANK2 := $(TARGETOBJDIR)/test-logic-dispatch-bank2
TEST_DISPATCH_RELEASE := $(TARGETOBJDIR)/test-logic-dispatch-release
TEST_TRACE := $(TARGETOBJDIR)/test-logic-trace
TEST_TRACE_ALLOCATOR := $(TARGETOBJDIR)/test-logic-trace-allocator
TEST_APP_REPLAY_LAYOUT := $(TARGETOBJDIR)/test-app-replay-layout
TEST_APP_REPLAY_REQUESTS := $(TARGETOBJDIR)/test-app-replay-requests
TEST_APP_REDUCE_REQUEST := $(TARGETOBJDIR)/test-app-reduce-request
TEST_REPLAY := $(TARGETOBJDIR)/test-logic-replay
TEST_REPLAY_TARGET := $(TARGETOBJDIR)/test-logic-replay
TEST_ORACLE := $(TARGETOBJDIR)/test-logic-oracle
TEST_REAL_GAME := $(TARGETOBJDIR)/test-real-game-corpus
TRACE_CLI := $(TARGETOBJDIR)/logic-trace
REPLAY_CLI := $(TARGETOBJDIR)/logic-replay
ORACLE_CLI := $(TARGETOBJDIR)/logic-oracle
SCUMMVM_SOURCE_DIR ?= $(TARGETOBJDIR)/scummvm-source
SCUMMVM_BUILD_DIR ?= $(TARGETOBJDIR)/scummvm-build-current
SCUMMVM_REVISION ?= e98793746f05344d0dad2c958a11921e14b34147
SCUMMVM_ORACLE := $(TARGETOBJDIR)/logic-oracle-scummvm
CX16_AGI_TEST_GAMES_DIR ?= $(CX16_GAMES_DIR)
HOSTTESTFLAGS := -std=c99 -Wall -Wextra -Werror -Itools
APP_REPLAY_DIR := obj/cx16/logic-app-replay
APP_REPLAY_PROGRAM := $(APP_REPLAY_DIR)/agi.cx16
APP_REPLAY_OBJDIR := $(APP_REPLAY_DIR)/obj
APP_REPLAY_TRACE_OBJECT := $(TARGETOBJDIR)/logic_trace.o
RELEASE_CHECK_DIR := obj/cx16/logic-app-release
RELEASE_CHECK_PROGRAM := $(RELEASE_CHECK_DIR)/agi.cx16
export CX16_LOGIC_RELEASE_PRG ?= $(RELEASE_CHECK_PROGRAM)

ifeq ($(APP_REPLAY_TARGET),1)
override CFLAGS += -D LOGIC_APP_REPLAY_TARGET=1 -D LOGIC_CONTEXT_TRACE=1 -D LOGIC_CONTEXT_VERIFY=1 -D LOGIC_CONTEXT_FAULT_INJECTION=1
override ASFLAGS += -Wa -D -Wa LOGIC_APP_REPLAY_TARGET=1 -Wa -D -Wa LOGIC_CONTEXT_TRACE=1
OBJECTS += $(APP_REPLAY_TRACE_OBJECT)
endif

.PHONY: app-replay test-games
.PHONY: app-release-check
app-release-check: $(RELEASE_CHECK_DIR)/Makefile.options
	$(MAKE) all TARGETS=cx16 PROGRAM=$(RELEASE_CHECK_DIR)/agi OBJDIR=$(RELEASE_CHECK_DIR)/obj APP_REPLAY_TARGET=0 OPTIONS=debugfile,labelfile STATEFILE=$(RELEASE_CHECK_DIR)/Makefile.options

$(RELEASE_CHECK_DIR):
	$(call MKDIR,$@)

$(RELEASE_CHECK_DIR)/Makefile.options: | $(RELEASE_CHECK_DIR)
	@printf '_OPTIONS_=debugfile,labelfile\n' > $@

app-replay: $(APP_REPLAY_DIR)/Makefile.options
	$(MAKE) all TARGETS=cx16 PROGRAM=$(APP_REPLAY_DIR)/agi OBJDIR=$(APP_REPLAY_OBJDIR) APP_REPLAY_TARGET=1 OPTIONS=debugfile,labelfile STATEFILE=$(APP_REPLAY_DIR)/Makefile.options

$(APP_REPLAY_DIR):
	$(call MKDIR,$@)

$(APP_REPLAY_DIR)/Makefile.options: | $(APP_REPLAY_DIR)
	@printf '_OPTIONS_=debugfile,labelfile\n' > $@

.PHONY: test-emulator
test-emulator: $(X16_TEST_EMULATOR)

ifeq ($(abspath $(X16EMU)),$(abspath $(X16_TEST_EMULATOR)))
test test-games: $(X16_TEST_EMULATOR)
endif

$(X16_EMULATOR_SOURCE_DIR)/CMakeLists.txt:
	mkdir -p "$(X16_EMULATOR_SOURCE_DIR)"
	git -C "$(X16_EMULATOR_SOURCE_DIR)" init
	git -C "$(X16_EMULATOR_SOURCE_DIR)" fetch --depth 1 https://github.com/X16Community/x16-emulator.git "$(X16_EMULATOR_REVISION)"
	git -C "$(X16_EMULATOR_SOURCE_DIR)" checkout --detach FETCH_HEAD

$(X16_EMULATOR_SOURCE_DIR)/.cx16-video-testbench: tools/x16_testbench_video.patch $(X16_EMULATOR_SOURCE_DIR)/CMakeLists.txt
	@if git -C "$(X16_EMULATOR_SOURCE_DIR)" apply --reverse --check "$(abspath tools/x16_testbench_video.patch)" >/dev/null 2>&1; then :; else git -C "$(X16_EMULATOR_SOURCE_DIR)" apply --check "$(abspath tools/x16_testbench_video.patch)" && git -C "$(X16_EMULATOR_SOURCE_DIR)" apply "$(abspath tools/x16_testbench_video.patch)"; fi
	touch "$@"

$(X16_EMULATOR_SOURCE_DIR)/build/CMakeCache.txt: $(X16_EMULATOR_SOURCE_DIR)/.cx16-video-testbench
	cmake -S "$(X16_EMULATOR_SOURCE_DIR)" -B "$(X16_EMULATOR_SOURCE_DIR)/build" -DCMAKE_BUILD_TYPE=Release -DENABLE_FLUIDSYNTH=OFF

$(X16_TEST_EMULATOR): $(X16_EMULATOR_SOURCE_DIR)/build/CMakeCache.txt
	cmake --build "$(X16_EMULATOR_SOURCE_DIR)/build" --target x16emu -j2

ifeq ($(APP_REPLAY_TARGET),1)
$(APP_REPLAY_TRACE_OBJECT): tools/logic_trace.c tools/logic_trace.h | $(TARGETOBJDIR)
	$(CC) -t $(CC65TARGET) -c $(CFLAGS) -o $@ $<

# The generic $(PROGRAM) prerequisites were expanded before the replay-only
# object list was extended. Add the object as a target prerequisite here so it
# is also present in the linker's automatic $^ input list.
$(PROGRAM): $(APP_REPLAY_TRACE_OBJECT)
endif

test: $(TEST_TRACE_ALLOCATOR).prg $(TEST_DISPATCH).prg $(TEST_DISPATCH_DIRECT).prg $(TEST_DISPATCH_BANK2).prg $(TEST_DISPATCH_RELEASE).prg $(TEST_REPLAY_TARGET).prg $(TEST_DISPATCH).runner $(TEST_TRACE) $(TEST_APP_REPLAY_LAYOUT) $(TEST_APP_REPLAY_REQUESTS) $(TEST_APP_REDUCE_REQUEST) $(TEST_REPLAY) $(TEST_ORACLE) $(TEST_REAL_GAME) $(TRACE_CLI) $(REPLAY_CLI) $(ORACLE_CLI) $(SCUMMVM_ORACLE)
	$(TEST_DISPATCH).runner "$(X16EMU)" "$(X16ROM)" "$(TEST_TRACE_ALLOCATOR).prg" "$(TEST_TRACE_ALLOCATOR).lbl" --trace-allocator
	$(TEST_DISPATCH).runner "$(X16EMU)" "$(X16ROM)" "$(TEST_DISPATCH).prg" "$(TEST_DISPATCH).lbl"
	$(TEST_DISPATCH).runner "$(X16EMU)" "$(X16ROM)" "$(TEST_DISPATCH_DIRECT).prg" "$(TEST_DISPATCH_DIRECT).lbl"
	$(TEST_DISPATCH).runner "$(X16EMU)" "$(X16ROM)" "$(TEST_DISPATCH_BANK2).prg" "$(TEST_DISPATCH_BANK2).lbl" --banked
	$(TEST_DISPATCH).runner "$(X16EMU)" "$(X16ROM)" "$(TEST_DISPATCH_RELEASE).prg" "$(TEST_DISPATCH_RELEASE).lbl"
	$(TEST_TRACE) "$(TRACE_CLI)"
	$(TEST_APP_REPLAY_LAYOUT)
	$(TEST_APP_REPLAY_REQUESTS)
	$(TEST_APP_REDUCE_REQUEST) "$(REPLAY_CLI)"
	X16EMU="$(X16EMU)" X16ROM="$(X16ROM)" CX16_LOGIC_REPLAY_PRG="$(TEST_REPLAY_TARGET).prg" CX16_LOGIC_REPLAY_LABELS="$(TEST_REPLAY_TARGET).lbl" $(TEST_REPLAY)
	X16EMU="$(X16EMU)" X16ROM="$(X16ROM)" CX16_LOGIC_REPLAY_PRG="$(TEST_REPLAY_TARGET).prg" CX16_LOGIC_REPLAY_LABELS="$(TEST_REPLAY_TARGET).lbl" CX16_AGI_TEST_GAMES_DIR="$(CX16_AGI_TEST_GAMES_DIR)" $(TEST_ORACLE)
	$(MAKE) test-fixture-cleanup TARGETS=cx16 CC65_HOME="$(CC65_HOME)" X16EMU="$(X16EMU)" X16ROM="$(X16ROM)"
	$(MAKE) test-games TARGETS=cx16 CC65_HOME="$(CC65_HOME)" X16EMU="$(X16EMU)" X16ROM="$(X16ROM)" CX16_GAMES_DIR="$(CX16_GAMES_DIR)"

$(TEST_DISPATCH).prg: tests/test_logic_dispatch.c tests/logic_dispatch_fixture.s tests/logic_dispatch_stubs.s tests/cx16-logic-test.cfg src/commandLoop.s src/logicCommands.s src/logicContext.c src/logicContext.h src/logicContext.s src/randomAsm.s src/modulus.s src/codeWindow.s src/global.s src/logicTypes.h src/general.h src/x16.inc tools/logic_trace.c tools/logic_trace.h | $(TARGETOBJDIR)
	$(CC) -t cx16 -D LOGIC_CONTEXT_VERIFY=1 -D LOGIC_CONTEXT_TRACE=1 -C tests/cx16-logic-test.cfg -I src -Wa -D -Wa LOGIC_CONTEXT_TRACE=1 -Wa --include-dir -Wa src -Ln $(TEST_DISPATCH).lbl --mapfile $(TEST_DISPATCH).map -o $@ tests/test_logic_dispatch.c src/logicContext.c tools/logic_trace.c tests/logic_dispatch_fixture.s tests/logic_dispatch_stubs.s

$(TEST_DISPATCH_DIRECT).prg: tests/test_logic_dispatch.c tests/logic_dispatch_fixture.s tests/logic_dispatch_stubs.s tests/cx16-logic-test.cfg src/commandLoop.s src/logicCommands.s src/logicContext.c src/logicContext.h src/logicContext.s src/randomAsm.s src/modulus.s src/codeWindow.s src/global.s src/logicTypes.h src/general.h src/x16.inc tools/logic_trace.c tools/logic_trace.h | $(TARGETOBJDIR)
	$(CC) -t cx16 -D LOGIC_DISPATCH_DIRECT_ONLY=1 -D LOGIC_CONTEXT_VERIFY=1 -D LOGIC_CONTEXT_TRACE=1 -C tests/cx16-logic-test.cfg -I src -Wa -D -Wa LOGIC_CONTEXT_TRACE=1 -Wa --include-dir -Wa src -Ln $(TEST_DISPATCH_DIRECT).lbl --mapfile $(TEST_DISPATCH_DIRECT).map -o $@ tests/test_logic_dispatch.c src/logicContext.c tools/logic_trace.c tests/logic_dispatch_fixture.s tests/logic_dispatch_stubs.s

$(TEST_DISPATCH_RELEASE).prg: tests/test_logic_dispatch.c tests/logic_dispatch_fixture.s tests/logic_dispatch_stubs.s tests/cx16-logic-test.cfg src/commandLoop.s src/logicCommands.s src/logicContext.c src/logicContext.h src/logicContext.s src/randomAsm.s src/modulus.s src/codeWindow.s src/global.s src/logicTypes.h src/general.h src/x16.inc tools/logic_trace.h | $(TARGETOBJDIR)
	$(CC) $(CFLAGS) -t cx16 -C tests/cx16-logic-test.cfg -I src -Wa --include-dir -Wa src -Ln $(TEST_DISPATCH_RELEASE).lbl --mapfile $(TEST_DISPATCH_RELEASE).map -o $@ tests/test_logic_dispatch.c src/logicContext.c tests/logic_dispatch_fixture.s tests/logic_dispatch_stubs.s

$(TEST_DISPATCH_BANK2).prg: tests/test_logic_dispatch.c tests/logic_dispatch_fixture.s tests/logic_dispatch_stubs.s tests/cx16-logic-dispatch-test.cfg src/commandLoop.s src/logicCommands.s src/logicContext.c src/logicContext.h src/logicContext.s src/randomAsm.s src/modulus.s src/codeWindow.s src/global.s src/logicTypes.h src/general.h src/x16.inc tools/logic_trace.c tools/logic_trace.h | $(TARGETOBJDIR)
	$(CC) -t cx16 -D LOGIC_DISPATCH_BANK2_RETURN_ONLY=1 -D LOGIC_CONTEXT_VERIFY=1 -D LOGIC_CONTEXT_TRACE=1 -C tests/cx16-logic-dispatch-test.cfg -I src -Wa -D -Wa LOGIC_CONTEXT_TRACE=1 -Wa --include-dir -Wa src -Ln $(TEST_DISPATCH_BANK2).lbl --mapfile $(TEST_DISPATCH_BANK2).map -o $@ tests/test_logic_dispatch.c src/logicContext.c tools/logic_trace.c tests/logic_dispatch_fixture.s tests/logic_dispatch_stubs.s

$(TEST_REPLAY_TARGET).prg: tests/test_logic_replay_target.c tests/logic_replay_fixture.s tests/logic_dispatch_stubs.s tests/cx16-logic-test.cfg src/commandLoop.s src/logicCommands.s src/logicContext.c src/logicContext.h src/logicContext.s src/logicTypes.h src/general.h src/x16.inc tools/logic_trace.c tools/logic_trace.h | $(TARGETOBJDIR)
	$(CC) -O -t cx16 -D LOGIC_REPLAY_TARGET=1 -D LOGIC_CONTEXT_VERIFY=1 -D LOGIC_CONTEXT_FAULT_INJECTION=1 -D LOGIC_CONTEXT_TRACE=1 -C tests/cx16-logic-test.cfg -I src -Wa -D -Wa LOGIC_CONTEXT_TRACE=1 -Wa --include-dir -Wa src -Ln $(TEST_REPLAY_TARGET).lbl --mapfile $(TEST_REPLAY_TARGET).map -o $@ tests/test_logic_replay_target.c src/logicContext.c tools/logic_trace.c tests/logic_replay_fixture.s tests/logic_dispatch_stubs.s

$(TEST_DISPATCH).runner: tests/x16_test_runner.c | $(TARGETOBJDIR)
	$(HOSTCC) -std=c99 -Wall -Wextra -Werror -o $@ $<

$(TEST_TRACE_ALLOCATOR).prg: tests/test_logic_trace_allocator.c tests/cx16-logic-trace-test.cfg tests/logic_trace_allocator_fixture.s src/logicContext.c src/logicContext.h src/logicTypes.h src/memoryManager.c src/memoryManager.h tools/logic_trace.c tools/logic_trace.h | $(TARGETOBJDIR)
	$(CC) -O -t cx16 -D LOGIC_APP_REPLAY_TARGET=1 -D LOGIC_CONTEXT_TRACE=1 -C tests/cx16-logic-trace-test.cfg -I src -Ln $(TEST_TRACE_ALLOCATOR).lbl --mapfile $(TEST_TRACE_ALLOCATOR).map -o $@ tests/test_logic_trace_allocator.c src/logicContext.c src/memoryManager.c tools/logic_trace.c tests/logic_trace_allocator_fixture.s

$(TEST_TRACE): tests/test_logic_trace.c tools/logic_trace.c tools/logic_trace.h tools/logic_app_replay.h | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tests/test_logic_trace.c tools/logic_trace.c

$(TEST_APP_REPLAY_LAYOUT): tests/test_app_replay_layout.c tools/logic_app_replay.h tools/logic_replay_runner.c tools/logic_replay_runner.h tools/logic_oracle.c tools/logic_oracle.h tools/logic_replay.c tools/logic_replay.h tools/logic_trace.c tools/logic_trace.h tests/x16_replay_runner.c tests/x16_replay_runner.h | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tests/test_app_replay_layout.c tools/logic_replay_runner.c tools/logic_oracle.c tools/logic_replay.c tools/logic_trace.c tests/x16_replay_runner.c

$(TEST_APP_REPLAY_REQUESTS): tests/test_app_replay_requests.c tests/x16_app_replay_runner.c tests/x16_replay_runner.h tools/logic_app_replay.h | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -Itests -o $@ tests/test_app_replay_requests.c tests/x16_app_replay_runner.c

$(TEST_APP_REDUCE_REQUEST): tests/test_app_reduce_request.c tools/logic_replay.c tools/logic_replay.h tools/logic_trace.c tools/logic_trace.h $(REPLAY_CLI) | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tests/test_app_reduce_request.c tools/logic_replay.c tools/logic_trace.c

$(TEST_REPLAY): tests/test_logic_replay.c tests/x16_replay_runner.c tests/x16_replay_runner.h tools/logic_replay.c tools/logic_replay.h tools/logic_replay_runner.c tools/logic_replay_runner.h tools/logic_reduce.c tools/logic_reduce.h tools/logic_trace.c tools/logic_trace.h | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tests/test_logic_replay.c tools/logic_replay.c tools/logic_replay_runner.c tests/x16_replay_runner.c tools/logic_reduce.c tools/logic_trace.c

$(TEST_ORACLE): tests/test_logic_oracle.c tools/logic_oracle.c tools/logic_oracle.h tools/logic_replay.c tools/logic_replay.h tools/logic_trace.c tools/logic_trace.h $(REPLAY_CLI) $(ORACLE_CLI) | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tests/test_logic_oracle.c tools/logic_oracle.c tools/logic_replay.c tools/logic_trace.c

$(TEST_REAL_GAME): tests/test_real_game_corpus.c tests/real_game_corpus.tsv tests/x16_app_replay_runner.c tests/x16_replay_runner.c tests/x16_replay_runner.h tools/logic_app_replay.h tools/logic_oracle.c tools/logic_oracle.h tools/logic_replay.c tools/logic_replay.h tools/logic_replay_runner.c tools/logic_replay_runner.h tools/logic_reduce.c tools/logic_reduce.h tools/logic_trace.c tools/logic_trace.h | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tests/test_real_game_corpus.c tools/logic_replay.c tools/logic_replay_runner.c tools/logic_reduce.c tests/x16_app_replay_runner.c tests/x16_replay_runner.c tools/logic_trace.c tools/logic_oracle.c

$(TRACE_CLI): tools/logic_trace_cli.c tools/logic_trace.c tools/logic_trace.h tools/logic_app_replay.h | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tools/logic_trace_cli.c tools/logic_trace.c

$(REPLAY_CLI): tools/logic_replay_main.c tests/x16_replay_runner.c tests/x16_app_replay_runner.c tests/x16_replay_runner.h tools/logic_app_replay.h tools/logic_replay.c tools/logic_replay.h tools/logic_replay_runner.c tools/logic_replay_runner.h tools/logic_reduce.c tools/logic_reduce.h tools/logic_trace.c tools/logic_trace.h tools/logic_oracle.c tools/logic_oracle.h | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tools/logic_replay_main.c tools/logic_replay.c tools/logic_replay_runner.c tests/x16_replay_runner.c tests/x16_app_replay_runner.c tools/logic_reduce.c tools/logic_trace.c tools/logic_oracle.c

$(SCUMMVM_SOURCE_DIR)/configure:
	mkdir -p "$(SCUMMVM_SOURCE_DIR)"
	git -C "$(SCUMMVM_SOURCE_DIR)" init
	git -C "$(SCUMMVM_SOURCE_DIR)" fetch --depth 1 https://github.com/scummvm/scummvm.git "$(SCUMMVM_REVISION)"
	git -C "$(SCUMMVM_SOURCE_DIR)" checkout --detach FETCH_HEAD

$(SCUMMVM_BUILD_DIR)/Makefile: $(SCUMMVM_SOURCE_DIR)/configure
	mkdir -p "$(SCUMMVM_BUILD_DIR)"
	cd "$(SCUMMVM_BUILD_DIR)" && "$(abspath $(SCUMMVM_SOURCE_DIR))/configure" --backend=null --disable-all-engines --enable-engine=agi --disable-detection-full

$(SCUMMVM_SOURCE_DIR)/.cx16-agi-cycle-hook: tools/logic_oracle_scummvm-cycle.patch $(SCUMMVM_SOURCE_DIR)/configure
	@if git -C "$(SCUMMVM_SOURCE_DIR)" apply --reverse --check "$(abspath tools/logic_oracle_scummvm-cycle.patch)" >/dev/null 2>&1; then :; else git -C "$(SCUMMVM_SOURCE_DIR)" apply --check "$(abspath tools/logic_oracle_scummvm-cycle.patch)" && git -C "$(SCUMMVM_SOURCE_DIR)" apply "$(abspath tools/logic_oracle_scummvm-cycle.patch)"; fi
	touch "$@"

$(SCUMMVM_SOURCE_DIR)/.cx16-agi-message-hook: tools/logic_oracle_scummvm-message.patch $(SCUMMVM_SOURCE_DIR)/configure
	@if git -C "$(SCUMMVM_SOURCE_DIR)" apply --reverse --check "$(abspath tools/logic_oracle_scummvm-message.patch)" >/dev/null 2>&1; then :; else git -C "$(SCUMMVM_SOURCE_DIR)" apply --check "$(abspath tools/logic_oracle_scummvm-message.patch)" && git -C "$(SCUMMVM_SOURCE_DIR)" apply "$(abspath tools/logic_oracle_scummvm-message.patch)"; fi
	touch "$@"

$(SCUMMVM_SOURCE_DIR)/.cx16-agi-timing-hook: tools/logic_oracle_scummvm-timing.patch $(SCUMMVM_SOURCE_DIR)/configure
	@if git -C "$(SCUMMVM_SOURCE_DIR)" apply --reverse --check "$(abspath tools/logic_oracle_scummvm-timing.patch)" >/dev/null 2>&1; then :; else git -C "$(SCUMMVM_SOURCE_DIR)" apply --check "$(abspath tools/logic_oracle_scummvm-timing.patch)" && git -C "$(SCUMMVM_SOURCE_DIR)" apply "$(abspath tools/logic_oracle_scummvm-timing.patch)"; fi
	touch "$@"

$(SCUMMVM_ORACLE): tools/logic_oracle_scummvm.mk tools/logic_oracle_scummvm.cpp tools/logic_oracle.c tools/logic_oracle.h tools/logic_replay.c tools/logic_replay.h tools/logic_trace.c tools/logic_trace.h $(SCUMMVM_BUILD_DIR)/Makefile $(SCUMMVM_SOURCE_DIR)/.cx16-agi-cycle-hook $(SCUMMVM_SOURCE_DIR)/.cx16-agi-message-hook $(SCUMMVM_SOURCE_DIR)/.cx16-agi-timing-hook
	$(MAKE) -C "$(SCUMMVM_BUILD_DIR)" -f "$(abspath tools/logic_oracle_scummvm.mk)" logic-oracle-scummvm ORACLE_REPO_ROOT="$(CURDIR)" ORACLE_OUTPUT_ROOT="$(abspath $(TARGETOBJDIR))"

$(ORACLE_CLI): tools/logic_oracle_main.c tools/logic_oracle.c tools/logic_oracle.h | $(TARGETOBJDIR)
	$(HOSTCC) $(HOSTTESTFLAGS) -o $@ tools/logic_oracle_main.c tools/logic_oracle.c

.PHONY: test-fixture-cleanup
test-fixture-cleanup: $(TEST_REPLAY) $(TEST_ORACLE) $(TEST_APP_REDUCE_REQUEST) $(TEST_TRACE) $(TRACE_CLI) | $(TARGETOBJDIR)
	@$(TEST_TRACE) "$(TRACE_CLI)" --fail-after-fixtures; status=$$?; test $$status -eq 2
	@test -z "$$(find $(TARGETOBJDIR) -maxdepth 1 -type d -name 'logic-trace-test-*' -print -quit)"
	@$(TEST_REPLAY) --fail-after-fixtures; status=$$?; test $$status -eq 2
	@CX16_AGI_TEST_GAMES_DIR="$(CX16_AGI_TEST_GAMES_DIR)" $(TEST_ORACLE) --fail-after-fixtures; status=$$?; test $$status -eq 2
	@$(TEST_APP_REDUCE_REQUEST) --fail-after-fixtures; status=$$?; test $$status -eq 2
	@test -z "$$(find $(TARGETOBJDIR) -maxdepth 1 -type d \( -name 'logic-replay-test-*' -o -name 'logic-oracle-test-*' \) -print -quit)"
	@test -z "$$(find $(TARGETOBJDIR) -maxdepth 1 -type d \( -name 'app-reduce-request-*' -o -name 'app-semantic-reduce-*' \) -print -quit)"

test-games: app-release-check app-replay $(TEST_REAL_GAME) $(REPLAY_CLI) $(TRACE_CLI) $(SCUMMVM_ORACLE)
	@CX16_GAMES_DIR="$(CX16_GAMES_DIR)" CX16_LOGIC_APP_DIR="$(APP_REPLAY_DIR)" CX16_LOGIC_APP_PRG="$(APP_REPLAY_PROGRAM)" CX16_LOGIC_APP_LABELS="$(APP_REPLAY_PROGRAM).lbl" $(TEST_REAL_GAME) tests/real_game_corpus.tsv --stage-only
	@test -z "$$(find $(TARGETOBJDIR) -maxdepth 1 -type d -name 'real-game-test-*' -print -quit)"
	@status=0; CX16_GAMES_DIR="$(CX16_GAMES_DIR)" CX16_LOGIC_APP_DIR="$(APP_REPLAY_DIR)" CX16_LOGIC_APP_PRG="$(APP_REPLAY_PROGRAM)" CX16_LOGIC_APP_LABELS="$(APP_REPLAY_PROGRAM).lbl" $(TEST_REAL_GAME) tests/real_game_corpus.tsv --fail-after-fixtures || status=$$?; test $$status -eq 2
	@test -z "$$(find $(TARGETOBJDIR) -maxdepth 1 -type d -name 'real-game-test-*' -print -quit)"
	@test -x "$(SCUMMVM_ORACLE)" || { echo "missing ScummVM AGI oracle: $(SCUMMVM_ORACLE)" >&2; exit 1; }
	@CX16_GAMES_DIR="$(CX16_GAMES_DIR)" CX16_LOGIC_APP_DIR="$(APP_REPLAY_DIR)" CX16_LOGIC_APP_PRG="$(APP_REPLAY_PROGRAM)" CX16_LOGIC_APP_LABELS="$(APP_REPLAY_PROGRAM).lbl" CX16_LOGIC_REPLAY_CLI="$(REPLAY_CLI)" CX16_SCUMMVM_ORACLE="$(SCUMMVM_ORACLE)" X16EMU="$(X16EMU)" X16ROM="$(X16ROM)" $(TEST_REAL_GAME) tests/real_game_corpus.tsv
	@test -z "$$(find $(TARGETOBJDIR) -maxdepth 1 -type d -name 'real-game-test-*' -print -quit)"

clean:
	$(call RMFILES,$(OBJECTS))
	$(call RMFILES,$(DEPENDS))
	$(call RMFILES,$(REMOVES))
	$(call RMFILES,$(PROGRAM))

else # $(words $(TARGETLIST)),1

all test clean:
	$(foreach t,$(TARGETLIST),$(MAKE) TARGETS=$t $@$(NEWLINE))

endif # $(words $(TARGETLIST)),1

OBJDIRLIST := $(wildcard $(OBJDIR)/*)

zap:
	$(foreach o,$(OBJDIRLIST),-$(call RMFILES,$o/*.o $o/*.d $o/*.lst)$(NEWLINE))
	$(foreach o,$(OBJDIRLIST),-$(call RMDIR,$o)$(NEWLINE))
	-$(call RMDIR,$(OBJDIR))
	-$(call RMFILES,$(basename $(PROGRAM)).* $(STATEFILE))

love:
	@echo "Not war, eh?"

###################################################################
###  Place your additional targets in the additional Makefiles  ###
### in the same directory - their names have to end with ".mk"! ###
###################################################################
-include *.mk
