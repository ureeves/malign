ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

SRC_DIR := $(ROOT_DIR)/src
TEST_DIR := $(ROOT_DIR)/test

CC ?= gcc
CFLAGS ?= -Wall -O2
OUT_DIR ?= $(ROOT_DIR)/build

TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BINS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(OUT_DIR)/%)

all: modules $(TEST_BINS)

modules:
	$(MAKE) -C $(SRC_DIR) MO=$(OUT_DIR) modules

modules_install:
	$(MAKE) -C $(SRC_DIR) MO=$(OUT_DIR) modules_install

clean:
	rm -rf $(OUT_DIR)

help:
	$(MAKE) -C $(SRC_DIR) MO=$(OUT_DIR) help

test: $(TEST_BINS)
	@for bin in $(TEST_BINS); do \
		$$bin; \
	done

$(OUT_DIR)/%: $(TEST_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(OUT_DIR):
	mkdir -p $@

.PHONY: all modules modules_install test clean help
