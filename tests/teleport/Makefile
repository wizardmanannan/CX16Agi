# Teleport tests. The teleport code is 65C02 assembly, so these run the
# production modules on sim65 (cc65's 65C02 simulator). Artifacts go to
# $(BUILD).
#   CC65_HOME          cc65 install (default: cl65, od65, sim65 on PATH)
#   GAME_KQ1 GAME_KQ3  game directories, for `rooms`
CC = cc
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror
BUILD ?= ../../build/tests/teleport
CC65 = $(if $(CC65_HOME),$(CC65_HOME)/bin/)
CL65 = $(CC65)cl65
OD65 = $(CC65)od65
SIM65 = $(CC65)sim65
SIM = -t sim65c02 -O -I../../src
SIM_LINK = -t sim65c02 -C support/teleport_sim.cfg
TELEPORT_S = $(wildcard ../../src/teleport/*/*.s)
SERVICES = $(BUILD)/teleport.o $(BUILD)/services.o

.PHONY: test cpu rooms footprint

test: cpu rooms

$(BUILD):
	mkdir -p $@

$(BUILD)/extract_collision: support/extract_collision.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/collision.inc: ../../src/viewAsm.s $(BUILD)/extract_collision
	$(BUILD)/extract_collision $< $@

$(BUILD)/teleport.o: support/teleport_cpu.s $(TELEPORT_S) $(BUILD)/collision.inc
	$(CL65) -t sim65c02 --asm-include-dir $(BUILD) --asm-include-dir ../../src -c -o $@ $<

$(BUILD)/services.o: support/teleport_services.c support/teleport_sim.h | $(BUILD)
	$(CL65) $(SIM) -c -o $@ $<

$(BUILD)/%.o: */%.c support/teleport_sim.h | $(BUILD)
	$(CL65) $(SIM) -c -o $@ $<

$(BUILD)/scan_ref.o: support/teleport_scan_ref.c support/teleport_sim.h | $(BUILD)
	$(CL65) $(SIM) -c -o $@ $<

$(BUILD)/test_teleport_cpu: $(BUILD)/test_teleport_cpu.o $(SERVICES)
	$(CL65) $(SIM_LINK) -o $@ $^

$(BUILD)/trigger_ref.o: support/teleport_trigger_ref.c support/teleport_sim.h | $(BUILD)
	$(CL65) $(SIM) -c -o $@ $<

$(BUILD)/test_teleport_rooms: $(BUILD)/test_teleport_rooms.o $(BUILD)/scan_ref.o $(BUILD)/trigger_ref.o $(SERVICES)
	$(CL65) $(SIM_LINK) -o $@ $^

cpu: $(BUILD)/test_teleport_cpu
	mkdir -p $(BUILD)/cpu-game
	$(SIM65) -c -x 2000000000 $< $(BUILD)/cpu-game

rooms: $(BUILD)/test_teleport_rooms
	$(if $(GAME_KQ1),,$(error set GAME_KQ1=/path/to/Kings-Quest-1))
	$(if $(GAME_KQ3),,$(error set GAME_KQ3=/path/to/Kings-Quest-3))
	$(SIM65) -c -x 2000000000 $< "$(GAME_KQ1)" kq1
	$(SIM65) -c -x 2000000000 $< "$(GAME_KQ3)" kq3

# Per-bank bytes of the teleport modules against budgets; with
# ENABLE_TELEPORT=0 no teleport symbol in the C objects or in main.s.
footprint: $(BUILD)/test_teleport_size $(BUILD)/teleport.o
	$(OD65) --dump-segsize $(BUILD)/teleport.o > $(BUILD)/teleport-segments.txt
	$(CL65) -t cx16 -O -D ENABLE_TELEPORT=0 -c -o $(BUILD)/parser-disabled.o ../../src/parser.c
	$(CL65) -t cx16 -O -D ENABLE_TELEPORT=0 -c -o $(BUILD)/meka-disabled.o ../../src/meka.c
	$(CL65) -t cx16 --asm-define ENABLE_TELEPORT=0 --asm-include-dir ../../src -c -o $(BUILD)/main-disabled.o ../../src/main.s
	$(OD65) --dump-imports --dump-exports $(BUILD)/parser-disabled.o > $(BUILD)/parser-disabled.txt
	$(OD65) --dump-imports --dump-exports $(BUILD)/meka-disabled.o > $(BUILD)/meka-disabled.txt
	$(OD65) --dump-imports --dump-exports $(BUILD)/main-disabled.o > $(BUILD)/main-disabled.txt
	$(BUILD)/test_teleport_size $(BUILD)/teleport-segments.txt $(BUILD)/parser-disabled.txt $(BUILD)/meka-disabled.txt $(BUILD)/main-disabled.txt

$(BUILD)/test_teleport_size: footprint/test_teleport_size.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@
