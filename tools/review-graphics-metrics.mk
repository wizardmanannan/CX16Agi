REPO_ROOT := $(abspath ..)
COMMON_DIR := $(shell git -C "$(REPO_ROOT)" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)
SHARED_TOOL_ROOT := $(shell dirname "$(COMMON_DIR)")
OUT_ROOT ?= $(REPO_ROOT)/obj/review-graphics-metrics
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CC65_HOME ?= $(SHARED_TOOL_ROOT)/.tools/cc65-latest
X16_EMULATOR_SOURCE ?= $(SHARED_TOOL_ROOT)/.tools/x16-emulator
FIXTURE_BASE ?= $(if $(TMPDIR),$(TMPDIR),/tmp)

export CC65_HOME X16_EMULATOR_SOURCE

.PHONY: review-graphics-metrics review-graphics-metrics-test

review-graphics-metrics: $(OUT_ROOT)/review-graphics-metrics

review-graphics-metrics-test: $(OUT_ROOT)/review-graphics-metrics $(OUT_ROOT)/review_metrics \
	$(OUT_ROOT)/test_review_graphics_metrics
	@set -eu; \
	mkdir -p "$(OUT_ROOT)"; \
	work=$$(mktemp -d "$(OUT_ROOT)/run.XXXXXX"); \
	fixtures=$$(mktemp -d "$(FIXTURE_BASE)/cx16agi-review-graphics-metrics.XXXXXX"); \
	cleanup() { status=$$?; trap - EXIT; \
	  if [ -d "$$fixtures" ]; then \
	    residual=$$(find "$$fixtures" -mindepth 1 -print -quit); \
	    if [ -n "$$residual" ]; then echo "review-graphics-metrics test: fixture remains: $$residual" >&2; status=2; \
	    elif ! rmdir "$$fixtures"; then echo "review-graphics-metrics test: cannot remove fixture root $$fixtures" >&2; status=2; fi; \
	  fi; \
	  if [ -d "$$work" ] && ! rm -r -- "$$work"; then echo "review-graphics-metrics test: cannot remove output $$work" >&2; status=2; fi; \
	  exit "$$status"; }; \
	trap cleanup EXIT; \
	cd "$(REPO_ROOT)"; \
	TEST_OUTPUT_DIR="$$work" TMPDIR="$$fixtures" \
	  "$(OUT_ROOT)/test_review_graphics_metrics" "$(OUT_ROOT)/review_metrics" \
	  "$(OUT_ROOT)/review-graphics-metrics"

$(OUT_ROOT)/review_metrics: $(REPO_ROOT)/tools/review_metrics.c $(REPO_ROOT)/tools/review_metrics.h \
	$(REPO_ROOT)/tools/review_sha256.c $(REPO_ROOT)/tools/review_sha256.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/review_metrics.c" "$(REPO_ROOT)/tools/review_sha256.c" -o "$@"

$(OUT_ROOT)/review-graphics-metrics: $(REPO_ROOT)/tools/review_graphics_metrics.c \
	$(REPO_ROOT)/tools/review_graphics_metrics.h $(REPO_ROOT)/tools/test_graphics_clear.c \
	$(REPO_ROOT)/tools/test_graphics_clear.h $(REPO_ROOT)/tools/review_sha256.c \
	$(REPO_ROOT)/tools/review_sha256.h $(X16_EMULATOR_SOURCE)/src/cpu/fake6502.c
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -DTEST_GRAPHICS_CLEAR_NO_MAIN -I"$(REPO_ROOT)/tools" \
	  -I"$(X16_EMULATOR_SOURCE)/src/cpu" "$(REPO_ROOT)/tools/review_graphics_metrics.c" \
	  "$(REPO_ROOT)/tools/test_graphics_clear.c" "$(REPO_ROOT)/tools/review_sha256.c" \
	  "$(X16_EMULATOR_SOURCE)/src/cpu/fake6502.c" -o "$@"

$(OUT_ROOT)/test_review_graphics_metrics: $(REPO_ROOT)/tools/review_graphics_metrics.c \
	$(REPO_ROOT)/tools/review_graphics_metrics.h $(REPO_ROOT)/tools/test_graphics_clear.c \
	$(REPO_ROOT)/tools/test_graphics_clear.h $(REPO_ROOT)/tools/review_sha256.c \
	$(REPO_ROOT)/tools/review_sha256.h $(X16_EMULATOR_SOURCE)/src/cpu/fake6502.c
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -DREVIEW_GRAPHICS_METRICS_TEST -DTEST_GRAPHICS_CLEAR_NO_MAIN \
	  -I"$(REPO_ROOT)/tools" -I"$(X16_EMULATOR_SOURCE)/src/cpu" \
	  "$(REPO_ROOT)/tools/review_graphics_metrics.c" "$(REPO_ROOT)/tools/test_graphics_clear.c" \
	  "$(REPO_ROOT)/tools/review_sha256.c" "$(X16_EMULATOR_SOURCE)/src/cpu/fake6502.c" -o "$@"
