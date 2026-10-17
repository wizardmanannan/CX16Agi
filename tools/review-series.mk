REPO_ROOT := $(abspath ..)
OUT_ROOT ?= $(REPO_ROOT)/obj/review-series
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
FIXTURE_BASE ?= $(if $(TMPDIR),$(TMPDIR),/tmp)

.PHONY: review-series-test review-series-core-test review-metrics-test review-events-test

review-series-test: review-series-core-test review-metrics-test review-events-test

review-series-core-test: $(OUT_ROOT)/review_series $(OUT_ROOT)/review_tools $(OUT_ROOT)/test_review_series
	@set -eu; \
	fixtures=$$(mktemp -d "$(FIXTURE_BASE)/cx16agi-review-series-suite.XXXXXX"); \
	cleanup() { \
	  status=$$?; trap - EXIT; \
	  if [ -d "$$fixtures" ]; then \
	    residual=$$(find "$$fixtures" -mindepth 1 -print -quit); \
	    if [ -n "$$residual" ]; then \
	      echo "review-series test: fixture was not cleaned: $$residual" >&2; status=2; \
	    elif ! rmdir "$$fixtures"; then \
	      echo "review-series test: could not clean fixture root $$fixtures" >&2; status=2; \
	    fi; \
	  fi; \
	  exit "$$status"; \
	}; \
	trap cleanup EXIT; \
	TEST_OUTPUT_DIR="$(OUT_ROOT)" TMPDIR="$$fixtures" \
	  "$(OUT_ROOT)/test_review_series" "$(OUT_ROOT)/review_series" "$(OUT_ROOT)/review_tools" "$(HOST_CC)" "$(REPO_ROOT)"

review-metrics-test: $(OUT_ROOT)/review_metrics $(OUT_ROOT)/test_review_metrics $(OUT_ROOT)/metrics_runner \
	$(OUT_ROOT)/metrics_runner_alt
	@set -eu; \
	fixtures=$$(mktemp -d "$(FIXTURE_BASE)/cx16agi-review-metrics-suite.XXXXXX"); \
	cleanup() { \
	  status=$$?; trap - EXIT; \
	  if [ -d "$$fixtures" ]; then \
	    residual=$$(find "$$fixtures" -mindepth 1 -print -quit); \
	    if [ -n "$$residual" ]; then \
	      echo "review-metrics test: fixture was not cleaned: $$residual" >&2; status=2; \
	    elif ! rmdir "$$fixtures"; then \
	      echo "review-metrics test: could not clean fixture root $$fixtures" >&2; status=2; \
	    fi; \
	  fi; \
	  exit "$$status"; \
	}; \
	trap cleanup EXIT; \
	TEST_OUTPUT_DIR="$(OUT_ROOT)" TMPDIR="$$fixtures" \
	  "$(OUT_ROOT)/test_review_metrics" "$(OUT_ROOT)/review_metrics" \
	  "$(OUT_ROOT)/metrics_runner" "$(OUT_ROOT)/metrics_runner_alt"

review-events-test: $(OUT_ROOT)/review_events.o $(OUT_ROOT)/review_events $(OUT_ROOT)/test_review_events
	@set -eu; \
	fixtures=$$(mktemp -d "$(FIXTURE_BASE)/cx16agi-review-events-suite.XXXXXX"); \
	cleanup() { \
	  status=$$?; trap - EXIT; \
	  if [ -d "$$fixtures" ]; then \
	    residual=$$(find "$$fixtures" -mindepth 1 -print -quit); \
	    if [ -n "$$residual" ]; then \
	      echo "review-events test: fixture was not cleaned: $$residual" >&2; status=2; \
	    elif ! rmdir "$$fixtures"; then \
	      echo "review-events test: could not clean fixture root $$fixtures" >&2; status=2; \
	    fi; \
	  fi; \
	  exit "$$status"; \
	}; \
	trap cleanup EXIT; \
	TEST_OUTPUT_DIR="$(OUT_ROOT)" TMPDIR="$$fixtures" "$(OUT_ROOT)/test_review_events" "$(OUT_ROOT)/review_events"

$(OUT_ROOT)/test_review_series: $(REPO_ROOT)/tools/test_review_series.c $(REPO_ROOT)/tools/test_paths.h \
	$(REPO_ROOT)/tools/test_run_from.h $(REPO_ROOT)/tools/review_format.h \
	$(REPO_ROOT)/tools/review_sha256.c $(REPO_ROOT)/tools/review_sha256.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/test_review_series.c" "$(REPO_ROOT)/tools/review_sha256.c" -o "$@"

$(OUT_ROOT)/review_series: $(REPO_ROOT)/tools/review_series.c $(REPO_ROOT)/tools/review_series.h \
	$(REPO_ROOT)/tools/review_format.h \
	$(REPO_ROOT)/tools/review_sha256.c $(REPO_ROOT)/tools/review_sha256.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/review_series.c" "$(REPO_ROOT)/tools/review_sha256.c" -o "$@"

$(OUT_ROOT)/review_tools: $(REPO_ROOT)/tools/review-tools.c $(REPO_ROOT)/tools/review_format.h \
	$(REPO_ROOT)/tools/review_sha256.c $(REPO_ROOT)/tools/review_sha256.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/review-tools.c" "$(REPO_ROOT)/tools/review_sha256.c" -o "$@"

$(OUT_ROOT)/review_metrics: $(REPO_ROOT)/tools/review_metrics.c $(REPO_ROOT)/tools/review_metrics.h \
	$(REPO_ROOT)/tools/review_sha256.c $(REPO_ROOT)/tools/review_sha256.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/review_metrics.c" "$(REPO_ROOT)/tools/review_sha256.c" -o "$@"

$(OUT_ROOT)/test_review_metrics: $(REPO_ROOT)/tools/test_review_metrics.c $(REPO_ROOT)/tools/test_paths.h \
	$(REPO_ROOT)/tools/test_run_from.h $(REPO_ROOT)/tools/review_sha256.c $(REPO_ROOT)/tools/review_sha256.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/test_review_metrics.c" "$(REPO_ROOT)/tools/review_sha256.c" -o "$@"

$(OUT_ROOT)/metrics_runner: $(REPO_ROOT)/tools/test_review_metrics.c $(REPO_ROOT)/tools/review_sha256.c \
	$(REPO_ROOT)/tools/review_sha256.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -DREVIEW_METRICS_FIXTURE_RUNNER -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/test_review_metrics.c" "$(REPO_ROOT)/tools/review_sha256.c" -o "$@"

$(OUT_ROOT)/metrics_runner_alt: $(REPO_ROOT)/tools/test_review_metrics.c $(REPO_ROOT)/tools/review_sha256.c \
	$(REPO_ROOT)/tools/review_sha256.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -DREVIEW_METRICS_FIXTURE_RUNNER -DMETRICS_RUNNER_VARIANT -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/test_review_metrics.c" "$(REPO_ROOT)/tools/review_sha256.c" -o "$@"

$(OUT_ROOT)/review_events.o: $(REPO_ROOT)/tools/review_events.c $(REPO_ROOT)/tools/review_events.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -DREVIEW_EVENTS_NO_MAIN -I"$(REPO_ROOT)/tools" -c \
	  "$(REPO_ROOT)/tools/review_events.c" -o "$@"

$(OUT_ROOT)/review_events: $(REPO_ROOT)/tools/review_events.c $(REPO_ROOT)/tools/review_events.h
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/review_events.c" -o "$@"

$(OUT_ROOT)/test_review_events: $(REPO_ROOT)/tools/test_review_events.c $(REPO_ROOT)/tools/review_events.h \
	$(REPO_ROOT)/tools/test_paths.h $(OUT_ROOT)/review_events.o
	@mkdir -p "$(OUT_ROOT)"
	$(HOST_CC) $(HOST_CFLAGS) -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/test_review_events.c" "$(OUT_ROOT)/review_events.o" -o "$@"
