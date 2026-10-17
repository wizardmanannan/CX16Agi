REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
HOST_CC ?= cc
HOST_CFLAGS ?= -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -save-temps=obj -fno-use-linker-plugin
TEST_OUTPUT_ROOT ?= $(REPO_ROOT)/obj
TEST_FIXTURE_ROOT ?= $(if $(TMPDIR),$(TMPDIR),/tmp)
WITNESS_OUT ?= $(REPO_ROOT)/obj/review-enforcement-witness

.PHONY: build test
build:
	@set -eu; \
	mkdir -p "$(WITNESS_OUT)"; \
	"$(HOST_CC)" -std=c11 -Wall -Wextra -Werror -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/review-tools.c" "$(REPO_ROOT)/tools/review_sha256.c" \
	  -o "$(WITNESS_OUT)/review-tools"; \
	"$(HOST_CC)" -std=c11 -Wall -Wextra -Werror -I"$(REPO_ROOT)/tools" \
	  "$(REPO_ROOT)/tools/test_review_evidence.c" "$(REPO_ROOT)/tools/review_sha256.c" \
	  -o "$(WITNESS_OUT)/test_review_evidence"

test:
	@set -eu; \
	repo='$(REPO_ROOT)'; output_base='$(TEST_OUTPUT_ROOT)'; fixture_base='$(TEST_FIXTURE_ROOT)'; \
	mkdir -p "$$output_base"; \
	output_base=$$(cd "$$output_base" && pwd -P); \
	fixture_base=$$(cd "$$fixture_base" && pwd -P); \
	case "$$output_base/" in "$$repo/.audit-tmp/"*) echo 'review test output must be outside .audit-tmp' >&2; exit 2 ;; esac; \
	case "$$fixture_base/" in "$$repo/"*) echo 'review test fixtures must be outside the worktree' >&2; exit 2 ;; esac; \
	case "$$fixture_base/" in "$$output_base/"*) echo 'review fixture root must be outside test output' >&2; exit 2 ;; esac; \
	case "$$output_base/" in "$$fixture_base/"*) echo 'review test output must be outside fixture root' >&2; exit 2 ;; esac; \
	work=$$(mktemp -d "$$output_base/review-enforcement.XXXXXX"); \
	fixture=$$(mktemp -d "$$fixture_base/cx16agi-review-enforcement.XXXXXX"); \
	cleanup() { status=$$?; trap - EXIT; if [ -n "$${fixture:-}" ] && [ -d "$$fixture" ]; then rm -rf -- "$$fixture"; fi; if [ -n "$${work:-}" ] && [ -d "$$work" ]; then rm -rf -- "$$work"; fi; exit "$$status"; }; \
	trap cleanup EXIT; \
	mkdir -p "$$work/bin"; \
	sanitized_preload=$${LD_PRELOAD:-}; \
	if [ -n "$$sanitized_preload" ]; then \
	  asan_runtime=$$("$(HOST_CC)" -print-file-name=libasan.so); \
	  [ -f "$$asan_runtime" ] || { echo 'host compiler has no ASan runtime for injected preloads' >&2; exit 2; }; \
	  sanitized_preload="$$asan_runtime:$$sanitized_preload"; \
	fi; \
	"$(HOST_CC)" $(HOST_CFLAGS) "$$repo/tools/review-tools.c" "$$repo/tools/review_sha256.c" -o "$$work/bin/review-tools"; \
	"$(HOST_CC)" $(HOST_CFLAGS) -I"$$repo/tools" "$$repo/tools/test_review_evidence.c" "$$repo/tools/review_sha256.c" -o "$$work/bin/test_review_evidence"; \
	cd "$$repo"; \
	env LD_PRELOAD="$$sanitized_preload" ASAN_OPTIONS=detect_leaks=0 TMPDIR="$$fixture" TEST_OUTPUT_DIR="$$work" \
	  "$$work/bin/test_review_evidence" "$$work/bin/review-tools" "$(HOST_CC)"
