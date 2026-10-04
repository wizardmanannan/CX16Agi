.DEFAULT_GOAL := test

REPO_ROOT := $(shell d='$(CURDIR)'; if [ -f "$$d/tools/test-image.mk" ]; then cd "$$d"; else cd "$$d/.."; fi && pwd -P)
# Linked worktrees share dependencies, but keep source and outputs local.
SHARED_TOOL_ROOT := $(shell d=$$(git -C "$(REPO_ROOT)" rev-parse --path-format=absolute --git-common-dir 2>/dev/null) && dirname "$$d")
CC65_HOME ?= $(SHARED_TOOL_ROOT)/.tools/cc65-latest
X16_EMULATOR_SOURCE ?= $(SHARED_TOOL_ROOT)/.tools/x16-emulator
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -O2 -Wall -Wextra
# Sanitized host children need ASan loaded first; omit the injected LD_PRELOAD for those runs.
TEST_OUTPUT_ROOT ?= $(REPO_ROOT)/obj

export CC65_HOME X16_EMULATOR_SOURCE TMPDIR ROM GAME BUILD SYMBOLS SDL_PREFIX KEEP_TEST_OUTPUT

.PHONY: test suite image production-test modes integration _run

test suite:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=core

image:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=image

production-test:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=production-test

modes integration:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=modes

_run:
	@set -eu; \
	repo='$(REPO_ROOT)'; mode='$(MODE)'; \
	repo=$$(cd "$$repo" && pwd -P); \
	fail() { echo "test suite: $$*" >&2; exit 2; }; \
	need_file() { [ -f "$$1" ] || fail "required file not found: $$1"; }; \
	need_exec() { [ -x "$$1" ] || fail "required executable not found: $$1"; }; \
	need_command() { command -v "$$1" >/dev/null 2>&1 || fail "required command not found: $$1"; }; \
	need_cc65() { \
	  [ -n "$${CC65_HOME:-}" ] || fail 'set CC65_HOME to a cc65 installation'; \
	  need_exec "$$CC65_HOME/bin/ca65"; need_exec "$$CC65_HOME/bin/ld65"; \
	}; \
	need_cpu() { \
	  [ -n "$${X16_EMULATOR_SOURCE:-}" ] || fail 'set X16_EMULATOR_SOURCE to the x16-emulator source directory'; \
	  need_file "$$X16_EMULATOR_SOURCE/src/cpu/fake6502.c"; \
	  need_file "$$X16_EMULATOR_SOURCE/src/cpu/fake6502.h"; \
	  need_file "$$X16_EMULATOR_SOURCE/src/cpu/registers.h"; \
	}; \
	need_image_makefile() { \
	  need_file "$$repo/tools/test-image.mk"; need_file "$$repo/tools/test_image_exports.s"; \
	}; \
	case "$$mode" in \
	  core) need_cc65; need_cpu ;; \
	  image) need_cc65; need_exec "$$CC65_HOME/bin/cl65"; need_image_makefile ;; \
	  production-test) \
	    need_cc65; need_exec "$$CC65_HOME/bin/cl65"; need_cpu; need_image_makefile; \
	    need_file "$$repo/tools/test_sound_foreground.c" ;; \
	  modes) \
	    need_command cmake; need_command magick; need_command pkg-config; \
	    if [ -n "$${SDL_PREFIX:-}" ]; then \
	      need_file "$$SDL_PREFIX/lib/cmake/SDL2/SDL2Config.cmake"; \
	    else pkg-config --exists sdl2 || fail 'SDL2 development files are required (pkg-config sdl2 or SDL_PREFIX)'; fi; \
	    pkg-config --exists libpng || fail 'libpng development files are required (pkg-config libpng)'; \
	    [ -n "$${ROM:-}" ] || fail 'set ROM to a compatible X16 ROM file'; \
	    [ -n "$${GAME:-}" ] || fail 'set GAME to the unpacked KQ1 game directory'; \
	    [ -n "$${BUILD:-}" ] || fail 'set BUILD to an existing production build with labels'; \
	    [ -n "$${SYMBOLS:-}" ] || fail 'set SYMBOLS to the production assembly debug-symbol file'; \
	    [ -n "$${X16_EMULATOR_SOURCE:-}" ] || fail 'set X16_EMULATOR_SOURCE to the x16-emulator source directory'; \
	    need_file "$$ROM"; [ -d "$$GAME" ] || fail "game directory not found: $$GAME"; \
	    [ -d "$$BUILD" ] || fail "build directory not found: $$BUILD"; need_file "$$SYMBOLS"; \
	    need_file "$$X16_EMULATOR_SOURCE/src/main.c"; need_file "$$repo/tools/test_full_game_irq.c" ;; \
	  *) fail "unknown run mode: $$mode" ;; \
	esac; \
	tmp_base='$(TEST_OUTPUT_ROOT)'; \
	mkdir -p "$$tmp_base"; \
	tmp_base=$$(cd "$$tmp_base" && pwd -P); \
	case "$$tmp_base/" in "$$repo/.audit-tmp/"*) fail 'TMPDIR must be outside .audit-tmp' ;; esac; \
	if [ -d "$$repo/.audit-tmp" ]; then \
	  audit_base=$$(cd "$$repo/.audit-tmp" && pwd -P); \
	  case "$$tmp_base/" in "$$audit_base/"*) fail 'TMPDIR must be outside .audit-tmp' ;; esac; \
	fi; \
	work=$$(mktemp -d "$$tmp_base/cx16agi-$$mode.XXXXXX") || fail 'could not create fresh output directory'; \
	mkdir -p "$$work/bin" "$$work/tmp"; export TMPDIR="$$work/tmp"; \
	echo "Output: $$work"; \
	if [ "$$mode" != image ] && [ "$${KEEP_TEST_OUTPUT:-0}" != 1 ]; then \
	  trap 'status=$$?; trap - EXIT; if ! rm -r -- "$$work"; then echo "test suite: could not clean owned output: $$work" >&2; exit 2; fi; exit "$$status"' EXIT; \
	fi; \
	case "$$mode" in \
	  core) \
	    cd "$$repo"; \
	    for name in graphics_clear irq_completion irq_helpers random screen_shake show_obj_irq text_irq words_assembly sprite_allocator; do \
	      echo "CC test_$$name.c"; \
	      "$(HOST_CC)" $(HOST_CFLAGS) -I"$$X16_EMULATOR_SOURCE/src/cpu" "$$repo/tools/test_$$name.c" "$$X16_EMULATOR_SOURCE/src/cpu/fake6502.c" -o "$$work/bin/test_$$name"; \
	    done; \
	    echo 'CC test_timer.c'; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas "$$repo/tools/test_timer.c" -o "$$work/bin/test_timer"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) "$$repo/tools/test_tool_paths.c" -o "$$work/bin/test_tool_paths"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas "$$repo/tools/test_palette_manager.c" -o "$$work/bin/test_palette_manager"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas -fsanitize=bounds -fno-sanitize-recover=bounds "$$repo/tools/test_controllers.c" -o "$$work/bin/test_controllers"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas -fsanitize=address,undefined -fno-sanitize-recover=all "$$repo/tools/test_menu.c" -o "$$work/bin/test_menu"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas -fsanitize=address,undefined -fno-sanitize-recover=all -idirafter "$$CC65_HOME/include" "$$repo/tools/test_text_buffers.c" -o "$$work/bin/test_text_buffers"; \
	    failed=0; \
	    run_test() { echo "RUN $$*"; if "$$@"; then :; else failed=$$((failed + 1)); fi; }; \
	    for name in graphics_clear irq_completion irq_helpers random show_obj_irq text_irq words_assembly sprite_allocator; do \
	      run_test "$$work/bin/test_$$name"; \
	    done; \
	    run_test "$$work/bin/test_screen_shake" --repo-root "$$repo"; \
	    run_test "$$work/bin/test_timer"; \
	    run_test "$$work/bin/test_tool_paths"; \
	    run_test "$$work/bin/test_palette_manager"; \
	    run_test "$$work/bin/test_controllers"; \
	    run_test env -u LD_PRELOAD "$$work/bin/test_menu"; \
	    run_test env -u LD_PRELOAD "$$work/bin/test_text_buffers"; \
	    [ "$$failed" -eq 0 ] || fail "$$failed regression(s) failed" ;; \
	  image|production-test) \
	    $(MAKE) --no-print-directory -f "$$repo/tools/test-image.mk" "REPO_ROOT=$$repo" "OUT=$$work/image"; \
	    if [ "$$mode" = production-test ]; then \
	      echo 'CC test_sound_foreground.c'; \
	      "$(HOST_CC)" $(HOST_CFLAGS) -I"$$X16_EMULATOR_SOURCE/src/cpu" "$$repo/tools/test_sound_foreground.c" "$$X16_EMULATOR_SOURCE/src/cpu/fake6502.c" -o "$$work/bin/test_sound_foreground"; \
	      echo 'RUN test_sound_foreground'; cd "$$repo"; "$$work/bin/test_sound_foreground" --build "$$work/image"; \
	    fi ;; \
	  modes) \
	    echo 'CC test_full_game_irq.c'; \
	    "$(HOST_CC)" $(HOST_CFLAGS) "$$repo/tools/test_full_game_irq.c" -lpng -o "$$work/bin/test_full_game_irq"; \
	    cd "$$repo"; set -- --emulator-source "$$X16_EMULATOR_SOURCE" --rom "$$ROM" --build "$$BUILD" --game "$$GAME" --symbols "$$SYMBOLS"; \
	    if [ -n "$${SDL_PREFIX:-}" ]; then set -- "$$@" --sdl-prefix "$$SDL_PREFIX"; fi; \
	    echo 'RUN test_full_game_irq --foreground-test'; "$$work/bin/test_full_game_irq" "$$@" --output "$$work/foreground" --foreground-test; \
	    echo 'RUN test_full_game_irq --picture-test'; "$$work/bin/test_full_game_irq" "$$@" --output "$$work/picture" --picture-test ;; \
	esac
