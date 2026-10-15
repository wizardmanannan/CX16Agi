.DEFAULT_GOAL := test

REPO_ROOT := $(shell d='$(CURDIR)'; if [ -f "$$d/tools/test-image.mk" ]; then cd "$$d"; else cd "$$d/.."; fi && pwd -P)
# Linked worktrees share dependencies, but keep source and outputs local.
SHARED_TOOL_ROOT := $(shell d=$$(git -C "$(REPO_ROOT)" rev-parse --path-format=absolute --git-common-dir 2>/dev/null) && dirname "$$d")
CC65_HOME ?= $(SHARED_TOOL_ROOT)/.tools/cc65-latest
X16_EMULATOR_SOURCE ?= $(SHARED_TOOL_ROOT)/.tools/x16-emulator
SDL_PREFIX ?= $(if $(wildcard $(SHARED_TOOL_ROOT)/.tools/sdl2-install/lib/cmake/SDL2/SDL2Config.cmake),$(SHARED_TOOL_ROOT)/.tools/sdl2-install)
TAS_CAPTURE_HEADER ?= $(SHARED_TOOL_ROOT)/projects/tool-assistance/src/core/tas-capture.h
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -O2 -Wall -Wextra
# Keep host compiler intermediates in the suite-owned output tree; GCC otherwise
# deletes temporary compiler/linker files outside it during test startup.
# Override HOST_TEST_FLAGS when using a non-GCC HOST_CC.
HOST_TEST_FLAGS ?= -save-temps=obj -fno-use-linker-plugin
override HOST_CFLAGS += $(HOST_TEST_FLAGS)
# Sanitized host children need ASan first while retaining injected command guards for fixture cleanup.
TEST_OUTPUT_ROOT ?= $(REPO_ROOT)/obj
TEST_FIXTURE_ROOT ?= $(if $(TMPDIR),$(TMPDIR),/tmp)

export CC65_HOME X16_EMULATOR_SOURCE TMPDIR TEST_OUTPUT_DIR ROM GAME BUILD SYMBOLS SDL_PREFIX TAS_CAPTURE_HEADER
unexport KEEP_TEST_OUTPUT

.PHONY: test suite commands-test image production-test integration-test picture-test loader-test modes integration _run

test suite:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=core

commands-test:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=commands-test

image:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=image

production-test:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=production-test

integration-test:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=integration-test

picture-test:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=picture-test

loader-test:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=loader-test

modes integration:
	@$(MAKE) --no-print-directory -f "$(REPO_ROOT)/tools/Makefile" _run MODE=modes

_run:
	@set -eu; \
	repo='$(REPO_ROOT)'; mode='$(MODE)'; keep_output='$(KEEP_TEST_OUTPUT)'; fixture_base='$(TEST_FIXTURE_ROOT)'; \
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
	  commands-test) need_cc65 ;; \
	  image) need_cc65; need_exec "$$CC65_HOME/bin/cl65"; need_image_makefile ;; \
	  production-test) \
	    need_cc65; need_exec "$$CC65_HOME/bin/cl65"; need_cpu; need_image_makefile; \
	    need_file "$$repo/tools/test_sound_foreground.c" ;; \
	  modes|integration-test|picture-test) \
	    need_command cmake; need_command magick; need_command pkg-config; \
	    if [ -n "$${SDL_PREFIX:-}" ]; then \
	      need_file "$$SDL_PREFIX/lib/cmake/SDL2/SDL2Config.cmake"; \
	    else pkg-config --exists sdl2 || fail 'SDL2 development files are required (pkg-config sdl2 or SDL_PREFIX)'; fi; \
	    pkg-config --exists libpng || fail 'libpng development files are required (pkg-config libpng)'; \
	    [ -n "$${ROM:-}" ] || fail 'set ROM to a compatible X16 ROM file'; \
	    [ -n "$${GAME:-}" ] || fail 'set GAME to the unpacked KQ1 game directory'; \
	    if [ "$$mode" = modes ]; then \
	      [ -n "$${BUILD:-}" ] || fail 'set BUILD to an existing production build with labels'; \
	      [ -n "$${SYMBOLS:-}" ] || fail 'set SYMBOLS to the production assembly debug-symbol file'; \
	    else need_cc65; need_exec "$$CC65_HOME/bin/cl65"; need_image_makefile; fi; \
	    [ -n "$${X16_EMULATOR_SOURCE:-}" ] || fail 'set X16_EMULATOR_SOURCE to the x16-emulator source directory'; \
	    need_file "$$ROM"; [ -d "$$GAME" ] || fail "game directory not found: $$GAME"; \
	    if [ "$$mode" = modes ]; then [ -d "$$BUILD" ] || fail "build directory not found: $$BUILD"; need_file "$$SYMBOLS"; fi; \
	    need_file "$$X16_EMULATOR_SOURCE/src/main.c"; need_file "$$repo/tools/test_full_game_irq.c" ;; \
	  loader-test) \
	    need_command cmake; need_command pkg-config; \
	    if [ -n "$${SDL_PREFIX:-}" ]; then need_file "$$SDL_PREFIX/lib/cmake/SDL2/SDL2Config.cmake"; \
	    else pkg-config --exists sdl2 || fail 'SDL2 development files are required (pkg-config sdl2 or SDL_PREFIX)'; fi; \
	    pkg-config --exists libpng || fail 'libpng development files are required (pkg-config libpng)'; \
	    [ -n "$${ROM:-}" ] || fail 'set ROM to a compatible X16 ROM file'; \
	    [ -n "$${GAME:-}" ] || fail 'set GAME to the unpacked KQ1 game directory'; \
	    need_cc65; need_exec "$$CC65_HOME/bin/cl65"; need_image_makefile; \
	    [ -n "$${X16_EMULATOR_SOURCE:-}" ] || fail 'set X16_EMULATOR_SOURCE to the x16-emulator source directory'; \
	    need_file "$$ROM"; [ -d "$$GAME" ] || fail "game directory not found: $$GAME"; \
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
	fixture_work=''; \
	cleanup_suite() { \
	  status=$$?; trap - EXIT; \
	  if [ "$$status" -ne 0 ]; then \
	    for log in "$$work"/foreground/build.log "$$work"/combined/build.log "$$work"/picture/build.log; do \
	      if [ -f "$$log" ]; then echo "--- $$log (last 80 lines) ---" >&2; tail -n 80 "$$log" >&2; fi; \
	    done; \
	  fi; \
	  if [ -n "$$fixture_work" ] && [ -d "$$fixture_work" ]; then \
	    residual_fixture=$$(find "$$fixture_work" -mindepth 1 -print -quit); \
	    if [ -n "$$residual_fixture" ]; then echo "test suite: test-owned fixture was not cleaned: $$residual_fixture" >&2; status=2; \
	    elif ! rmdir "$$fixture_work"; then echo "test suite: could not clean fixture root: $$fixture_work" >&2; status=2; fi; \
	  fi; \
	  if [ "$$mode" != image ] && [ "$$keep_output" != 1 ]; then \
	    if ! rm -r -- "$$work"; then echo "test suite: could not clean owned output: $$work" >&2; status=2; fi; \
	  fi; \
	  exit "$$status"; \
	}; \
	trap cleanup_suite EXIT; \
	[ -d "$$fixture_base" ] || fail "test fixture root not found: $$fixture_base"; \
	fixture_base=$$(cd "$$fixture_base" && pwd -P); \
	case "$$fixture_base/" in "$$repo/"*) fail 'test fixtures must be outside the product worktree' ;; esac; \
	case "$$fixture_base/" in "$$repo/.audit-tmp/"*) fail 'test fixtures must be outside .audit-tmp' ;; esac; \
	case "$$tmp_base/" in "$$fixture_base/"*) fail 'test output root must be outside test fixture root' ;; esac; \
	case "$$fixture_base/" in "$$tmp_base/"*) fail 'test fixture root must be outside test output root' ;; esac; \
	fixture_work=$$(mktemp -d "$$fixture_base/cx16agi-fixtures-$$mode.XXXXXX") || fail 'could not create fresh test fixture root'; \
	mkdir -p "$$work/bin"; TEST_OUTPUT_DIR="$$work"; export TMPDIR="$$fixture_work" TEST_OUTPUT_DIR; \
	echo "Output: $$work"; \
	build_commands() { \
	  for name in command_bounds parser_buffers; do \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas -Wno-error=incompatible-pointer-types -Wno-error=int-conversion -D_MSC_VER -D__CBM__ '-D__fastcall__=' '-D__asm__(...)=' -idirafter "$$CC65_HOME/include" -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-sanitize-recover=all "$$repo/tools/test_$$name.c" -Wl,--gc-sections -o "$$work/bin/test_$$name"; \
	  done; \
	}; \
	prepare_sanitizers() { \
	  sanitized_preload=$${LD_PRELOAD:-}; \
	  if [ -n "$$sanitized_preload" ]; then \
	    asan_runtime=$$("$(HOST_CC)" -print-file-name=libasan.so); \
	    [ -f "$$asan_runtime" ] || fail 'host compiler has no ASan shared runtime for injected preloads'; \
	    sanitized_preload="$$asan_runtime:$$sanitized_preload"; \
	  fi; \
	}; \
	case "$$mode" in \
	  commands-test) \
	    build_commands; prepare_sanitizers; failed=0; \
	    for option in '' --parse --strings --allocation; do \
	      if env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_command_bounds" $${option:+"$$option"}; then :; else failed=$$((failed + 1)); fi; \
	    done; \
	    for option in '' --unterminated; do \
	      if env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_parser_buffers" $${option:+"$$option"}; then :; else failed=$$((failed + 1)); fi; \
	    done; \
	    [ "$$failed" -eq 0 ] || fail "$$failed command regression(s) failed" ;; \
	  core) \
	    cd "$$repo"; \
	    for name in graphics_clear irq_completion irq_helpers random screen_shake show_obj_irq text_irq sprite_allocator; do \
	      echo "CC test_$$name.c"; \
	      "$(HOST_CC)" $(HOST_CFLAGS) -I"$$X16_EMULATOR_SOURCE/src/cpu" "$$repo/tools/test_$$name.c" "$$X16_EMULATOR_SOURCE/src/cpu/fake6502.c" -o "$$work/bin/test_$$name"; \
	    done; \
	    echo 'CC test_timer.c'; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas "$$repo/tools/test_timer.c" -o "$$work/bin/test_timer"; \
	    echo 'CC test_run_from.c'; \
	    "$(HOST_CC)" $(HOST_CFLAGS) "$$repo/tools/test_run_from.c" -o "$$work/bin/test_run_from"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) "$$repo/tools/test_tool_paths.c" -o "$$work/bin/test_tool_paths"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) "$$repo/tools/test_fixture_cleanup.c" -o "$$work/bin/test_fixture_cleanup"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas "$$repo/tools/test_palette_manager.c" -o "$$work/bin/test_palette_manager"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas -fsanitize=bounds -fno-sanitize-recover=bounds "$$repo/tools/test_controllers.c" -o "$$work/bin/test_controllers"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas -fsanitize=address,undefined -fno-sanitize-recover=all "$$repo/tools/test_menu.c" -o "$$work/bin/test_menu"; \
	    build_commands; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-unknown-pragmas -fsanitize=address,undefined -fno-sanitize-recover=all -idirafter "$$CC65_HOME/include" "$$repo/tools/test_text_buffers.c" -o "$$work/bin/test_text_buffers"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-error=incompatible-pointer-types -Wno-error=int-conversion -D_MSC_VER -D__CBM__ '-D__fastcall__=' '-D__asm__(...)=' -idirafter "$$CC65_HOME/include" -Wno-unknown-pragmas -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-sanitize-recover=all "$$repo/tools/test_words_file.c" -Wl,--gc-sections -o "$$work/bin/test_words_file"; \
	    "$(HOST_CC)" $(HOST_CFLAGS) -Wno-error=incompatible-pointer-types -Wno-error=int-conversion -D_MSC_VER -D__CBM__ '-D__fastcall__=' '-D__asm__(...)=' -idirafter "$$CC65_HOME/include" -Wno-unknown-pragmas -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-sanitize-recover=all "$$repo/tools/test_agifiles.c" -Wl,--gc-sections -o "$$work/bin/test_agifiles"; \
	    failed=0; \
	    run_test() { echo "RUN $$*"; if "$$@"; then :; else failed=$$((failed + 1)); fi; }; \
	    for name in graphics_clear irq_completion irq_helpers random show_obj_irq text_irq sprite_allocator; do \
	      run_test "$$work/bin/test_$$name"; \
	    done; \
	    run_test "$$work/bin/test_screen_shake" --repo-root "$$repo"; \
	    run_test "$$work/bin/test_timer"; \
	    run_test "$$work/bin/test_run_from"; \
	    run_test "$$work/bin/test_tool_paths"; \
	    run_test "$$work/bin/test_fixture_cleanup"; \
	    run_test "$$work/bin/test_palette_manager"; \
	    run_test "$$work/bin/test_controllers"; \
	    prepare_sanitizers; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_menu"; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_command_bounds"; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_command_bounds" --parse; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_command_bounds" --strings; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_command_bounds" --allocation; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_parser_buffers"; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_parser_buffers" --unterminated; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_text_buffers"; \
	    for option in --truncated-synonym --prefix-bound --full-word-order --short-header --shared-prefix --allocation-failure --staging-read --retry --first-nonzero --word-count --discard-reload --missing-file --trailing-sentinel; do \
	      run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_words_file" "$$option"; \
	    done; \
	    run_test env LD_PRELOAD="$$sanitized_preload" "$$work/bin/test_agifiles"; \
	    [ "$$failed" -eq 0 ] || fail "$$failed regression(s) failed" ;; \
	  image|production-test) \
	    $(MAKE) --no-print-directory -f "$$repo/tools/test-image.mk" "REPO_ROOT=$$repo" "OUT=$$work/image"; \
	    if [ "$$mode" = production-test ]; then \
	      failed=0; \
	      for name in sound_foreground banked_strings; do \
	        echo "CC test_$$name.c"; \
	        "$(HOST_CC)" $(HOST_CFLAGS) -I"$$X16_EMULATOR_SOURCE/src/cpu" "$$repo/tools/test_$$name.c" "$$X16_EMULATOR_SOURCE/src/cpu/fake6502.c" -o "$$work/bin/test_$$name"; \
	        cd "$$repo"; \
	        if [ "$$name" = banked_strings ]; then \
	          for test_case in --smoke --bounded --input --metadata --allocator-oom --get-num --object-commands --object-crypto-bounds --object-crypto-pointer --show-object --sound --text-cleanup --timer; do \
	            echo "RUN test_banked_strings $$test_case"; \
	            if "$$work/bin/test_banked_strings" --build "$$work/image" "$$test_case"; then :; else failed=$$((failed + 1)); fi; \
	          done; \
	        else \
	          echo "RUN test_$$name"; \
	          if "$$work/bin/test_$$name" --build "$$work/image"; then :; else failed=$$((failed + 1)); fi; \
	        fi; \
	      done; \
	      [ "$$failed" -eq 0 ] || fail "$$failed production regression(s) failed"; \
	    fi ;; \
	  modes|integration-test|picture-test) \
	    if [ "$$mode" = integration-test ] || [ "$$mode" = picture-test ]; then \
	      $(MAKE) --no-print-directory -f "$$repo/tools/test-image.mk" "REPO_ROOT=$$repo" "OUT=$$work/image"; \
	      BUILD="$$work/image"; SYMBOLS="$$work/image/final.lbl"; \
	    fi; \
	    echo 'CC test_full_game_irq.c'; \
	    "$(HOST_CC)" $(HOST_CFLAGS) "$$repo/tools/test_full_game_irq.c" -lpng -o "$$work/bin/test_full_game_irq"; \
	    cd "$$repo"; set -- --emulator-source "$$X16_EMULATOR_SOURCE" --rom "$$ROM" --build "$$BUILD" --game "$$GAME" --symbols "$$SYMBOLS"; \
	    if [ -n "$${TAS_CAPTURE_HEADER:-}" ]; then set -- "$$@" --tas-capture-header "$$TAS_CAPTURE_HEADER"; fi; \
	    if [ -n "$${SDL_PREFIX:-}" ]; then set -- "$$@" --sdl-prefix "$$SDL_PREFIX"; fi; \
	    if [ "$$mode" != picture-test ]; then \
	      echo 'RUN test_full_game_irq --foreground-test'; "$$work/bin/test_full_game_irq" "$$@" --output "$$work/foreground" --foreground-test; \
	    fi; \
	    if [ "$$mode" = integration-test ]; then \
	      echo 'RUN test_full_game_irq --combined-irq-test'; "$$work/bin/test_full_game_irq" "$$@" --output "$$work/combined" --combined-irq-test; \
	    fi; \
	    echo 'RUN test_full_game_irq --picture-test'; "$$work/bin/test_full_game_irq" "$$@" --output "$$work/picture" --picture-test ;; \
	  loader-test) \
	    $(MAKE) --no-print-directory -f "$$repo/tools/test-image.mk" "REPO_ROOT=$$repo" "OUT=$$work/image"; \
	    BUILD="$$work/image"; SYMBOLS="$$work/image/final.lbl"; \
	    echo 'CC test_full_game_irq.c'; \
	    "$(HOST_CC)" $(HOST_CFLAGS) "$$repo/tools/test_full_game_irq.c" -lpng -o "$$work/bin/test_full_game_irq"; \
	    cd "$$repo"; set -- --emulator-source "$$X16_EMULATOR_SOURCE" --rom "$$ROM" --build "$$BUILD" --game "$$GAME" --symbols "$$SYMBOLS" --loader-test; \
	    if [ -n "$${TAS_CAPTURE_HEADER:-}" ]; then set -- "$$@" --tas-capture-header "$$TAS_CAPTURE_HEADER"; fi; \
	    if [ -n "$${SDL_PREFIX:-}" ]; then set -- "$$@" --sdl-prefix "$$SDL_PREFIX"; fi; \
	    echo 'RUN test_full_game_irq --loader-test'; "$$work/bin/test_full_game_irq" "$$@" --output "$$work/loader" ;; \
	esac
