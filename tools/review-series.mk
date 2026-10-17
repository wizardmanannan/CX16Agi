REPO_ROOT := $(abspath ..)
OUT_ROOT ?= $(REPO_ROOT)/obj/review-series
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
FIXTURE_BASE ?= $(if $(TMPDIR),$(TMPDIR),/tmp)

.PHONY: review-events-test

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
