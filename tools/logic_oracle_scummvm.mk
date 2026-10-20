# Build the reference adapter from an out-of-tree ScummVM build configured
# with --backend=null --disable-all-engines --enable-engine=agi
# --disable-detection-full.
ORACLE_REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
ORACLE_BUILD_DIR := $(CURDIR)
ORACLE_OUTPUT_ROOT ?= $(ORACLE_REPO_ROOT)/obj/cx16
ORACLE_BINARY := $(ORACLE_OUTPUT_ROOT)/logic-oracle-scummvm
ORACLE_OBJECT_DIR := $(ORACLE_OUTPUT_ROOT)/scummvm-adapter
ORACLE_CXX_OBJECT := $(ORACLE_OBJECT_DIR)/logic_oracle_scummvm.o
ORACLE_C_OBJECT := $(ORACLE_OBJECT_DIR)/logic_oracle.o
ORACLE_REPLAY_OBJECT := $(ORACLE_OBJECT_DIR)/logic_replay.o
ORACLE_TRACE_OBJECT := $(ORACLE_OBJECT_DIR)/logic_trace.o

include $(ORACLE_BUILD_DIR)/Makefile

# Capture only after AGI finishes a cycle and clears its input flags.
engines/agi/cycle.o: CPPFLAGS += -DCX16_AGI_ORACLE_CYCLE_HOOK
engines/agi/text.o: CPPFLAGS += -DCX16_AGI_ORACLE_MESSAGE_HOOK
engines/agi/agi.o: CPPFLAGS += -DCX16_AGI_ORACLE_SEMANTIC_CLOCK

ORACLE_SCUMMVM_OBJECTS := $(filter-out backends/platform/null/null.o,\
	$(sort $(DETECT_OBJS) $(OBJS)))

.PHONY: logic-oracle-scummvm
logic-oracle-scummvm: $(ORACLE_BINARY)

$(ORACLE_OBJECT_DIR):
	mkdir -p $@

$(ORACLE_CXX_OBJECT): $(ORACLE_REPO_ROOT)/tools/logic_oracle_scummvm.cpp \
                      $(ORACLE_REPO_ROOT)/tools/logic_oracle.h \
                      $(ORACLE_REPO_ROOT)/tools/logic_replay.h \
                      $(srcdir)/backends/platform/null/null.cpp | $(ORACLE_OBJECT_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -I$(srcdir) -I$(ORACLE_REPO_ROOT)/tools -c $< -o $@

$(ORACLE_C_OBJECT): $(ORACLE_REPO_ROOT)/tools/logic_oracle.c \
                    $(ORACLE_REPO_ROOT)/tools/logic_oracle.h | $(ORACLE_OBJECT_DIR)
	$(CC) -std=c99 -Wall -Wextra -I$(ORACLE_REPO_ROOT)/tools -c $< -o $@

$(ORACLE_REPLAY_OBJECT): $(ORACLE_REPO_ROOT)/tools/logic_replay.c \
                         $(ORACLE_REPO_ROOT)/tools/logic_replay.h \
                         $(ORACLE_REPO_ROOT)/tools/logic_trace.h | $(ORACLE_OBJECT_DIR)
	$(CC) -std=c99 -Wall -Wextra -I$(ORACLE_REPO_ROOT)/tools -c $< -o $@

$(ORACLE_TRACE_OBJECT): $(ORACLE_REPO_ROOT)/tools/logic_trace.c \
	                        $(ORACLE_REPO_ROOT)/tools/logic_trace.h | $(ORACLE_OBJECT_DIR)
	$(CC) -std=c99 -Wall -Wextra -I$(ORACLE_REPO_ROOT)/tools -c $< -o $@

$(ORACLE_BINARY): $(ORACLE_SCUMMVM_OBJECTS) $(ORACLE_CXX_OBJECT) \
                  $(ORACLE_C_OBJECT) $(ORACLE_REPLAY_OBJECT) \
                  $(ORACLE_TRACE_OBJECT)
	$(LD) $(LDFLAGS) -Wl,--start-group $(ORACLE_SCUMMVM_OBJECTS) $(ORACLE_CXX_OBJECT) \
	    $(ORACLE_C_OBJECT) $(ORACLE_REPLAY_OBJECT) $(ORACLE_TRACE_OBJECT) \
	    -Wl,--end-group $(LIBS) -o $@
