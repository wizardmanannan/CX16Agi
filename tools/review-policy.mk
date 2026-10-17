REVIEW_POLICY_TOOLS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REVIEW_POLICY_REPO_ROOT := $(abspath $(REVIEW_POLICY_TOOLS_DIR)/..)
REVIEW_POLICY_OUTPUT_ROOT ?= $(REVIEW_POLICY_REPO_ROOT)/obj
REVIEW_POLICY_HOST_CC ?= cc

.PHONY: review-policy-test

review-policy-test:
	@set -eu; \
	output_root='$(REVIEW_POLICY_OUTPUT_ROOT)'; \
	mkdir -p "$$output_root"; \
	work=$$(mktemp -d "$$output_root/review-policy.XXXXXX"); \
	cleanup() { \
	  exit_status=$$?; \
	  trap - EXIT; \
	  if ! rm -rf -- "$$work"; then \
	    printf '%s\n' "Could not clean review policy output directory $$work." >&2; \
	    exit_status=1; \
	  fi; \
	  exit "$$exit_status"; \
	}; \
	trap cleanup EXIT; \
	mkdir "$$work/fixtures"; \
	'$(REVIEW_POLICY_HOST_CC)' -std=c11 -O2 -Wall -Wextra -Werror -DREVIEW_COMMAND_TIMEOUT_MS=1000 \
	  '$(REVIEW_POLICY_TOOLS_DIR)/review_policy.c' \
	  -o "$$work/review-policy"; \
	'$(REVIEW_POLICY_HOST_CC)' -std=c11 -O2 -Wall -Wextra -Werror \
	  -I'$(REVIEW_POLICY_TOOLS_DIR)' '$(REVIEW_POLICY_TOOLS_DIR)/test_review_policy.c' \
	  -o "$$work/test_review_policy"; \
	TEST_OUTPUT_DIR="$$work" TMPDIR="$$work/fixtures" \
	  "$$work/test_review_policy" "$$work/review-policy" \
	  '$(REVIEW_POLICY_TOOLS_DIR)/review/branch-protection.json'; \
	residual=$$(find "$$work" -mindepth 1 -maxdepth 1 -type d -name 'review-policy-test-*' -print -quit); \
	if [ -n "$$residual" ]; then \
	  printf '%s\n' 'Review policy test left an obj output directory after success.' >&2; exit 1; \
	fi; \
	residual=$$(find "$$work/fixtures" -mindepth 1 -type d -name 'cx16agi-review-policy-protocol-*' -print -quit); \
	if [ -n "$$residual" ]; then \
	  printf '%s\n' 'Review policy test left a fixture directory after success.' >&2; exit 1; \
	fi; \
	if TEST_OUTPUT_DIR="$$work" TMPDIR="$$work/fixtures" \
	  "$$work/test_review_policy" "$$work/missing-review-policy" \
	  '$(REVIEW_POLICY_TOOLS_DIR)/review/branch-protection.json' >/dev/null 2>&1; then \
	  printf '%s\n' 'Review policy failure cleanup probe unexpectedly passed.' >&2; exit 1; \
	fi; \
	residual=$$(find "$$work" -mindepth 1 -maxdepth 1 -type d -name 'review-policy-test-*' -print -quit); \
	if [ -n "$$residual" ]; then \
	  printf '%s\n' 'Review policy test left an obj output directory after failure.' >&2; exit 1; \
	fi; \
	residual=$$(find "$$work/fixtures" -mindepth 1 -type d -name 'cx16agi-review-policy-protocol-*' -print -quit); \
	if [ -n "$$residual" ]; then \
	  printf '%s\n' 'Review policy test left a fixture directory after failure.' >&2; exit 1; \
	fi; \
	printf '%s\n' 'PASS review-policy test cleanup after success and expected failure'
