CC = cc
CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Werror -Wno-unknown-pragmas
BUILD ?= ../build/tests/teleport
GAMES ?=
CL65 ?= cl65
OD65 ?= od65
SIM65 ?= sim65

.PHONY: test sanitize footprint cpu

test: $(BUILD)/test_teleport
	$(BUILD)/test_teleport $(GAMES)

$(BUILD):
	mkdir -p $@

$(BUILD)/test_teleport: test_teleport.c support/teleport_host.h ../src/teleport.c $(wildcard ../src/*.h) | $(BUILD)
	$(CC) $(CFLAGS) -I../src -include support/teleport_host.h test_teleport.c ../src/teleport.c -o $@

sanitize:
	$(MAKE) test BUILD=$(BUILD)/sanitize CFLAGS='$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer' GAMES='$(GAMES)'

$(BUILD)/test_teleport_size: test_teleport_size.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

# Inspect real cc65 objects, not host sizeof results or object-file disk size.
footprint: $(BUILD)/test_teleport_size
	$(CL65) -t cx16 -O -D ENABLE_TELEPORT=1 -c -o $(BUILD)/enabled.o ../src/teleport.c
	$(CL65) -t cx16 -O -D ENABLE_TELEPORT=0 -c -o $(BUILD)/disabled.o ../src/teleport.c
	$(CL65) -t cx16 -O -D ENABLE_TELEPORT=0 -c -o $(BUILD)/parser-disabled.o ../src/parser.c
	$(CL65) -t cx16 -O -D ENABLE_TELEPORT=0 -c -o $(BUILD)/meka-disabled.o ../src/meka.c
	$(OD65) --dump-segments $(BUILD)/enabled.o > $(BUILD)/enabled.txt
	$(OD65) --dump-segments --dump-imports --dump-exports $(BUILD)/disabled.o > $(BUILD)/disabled.txt
	$(OD65) --dump-imports --dump-exports $(BUILD)/parser-disabled.o > $(BUILD)/parser-disabled.txt
	$(OD65) --dump-imports --dump-exports $(BUILD)/meka-disabled.o > $(BUILD)/meka-disabled.txt
	$(BUILD)/test_teleport_size $(BUILD)/enabled.txt $(BUILD)/disabled.txt $(BUILD)/parser-disabled.txt $(BUILD)/meka-disabled.txt

$(BUILD)/extract_collision: extract_collision.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

cpu: $(BUILD)/extract_collision
	$(BUILD)/extract_collision ../src/viewAsm.s $(BUILD)/collision.inc
	$(CL65) -t sim65c02 -O -I../src -c -o $(BUILD)/cpu-test.o test_teleport_cpu.c
	$(CL65) -t sim65c02 -O -I../src -c -o $(BUILD)/cpu-teleport.o support/teleport_cpu_unit.c
	$(CL65) -t sim65c02 --asm-include-dir $(BUILD) -c -o $(BUILD)/cpu-collision.o support/teleport_cpu.s
	$(CL65) -t sim65c02 -C support/teleport_sim.cfg -o $(BUILD)/cpu-test $(BUILD)/cpu-test.o $(BUILD)/cpu-teleport.o $(BUILD)/cpu-collision.o
	$(SIM65) -c -x 10000000 $(BUILD)/cpu-test
