M := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

KDIR ?= /lib/modules/$(shell uname -r)/build
MO ?= $(M)/build

CC ?= gcc
TEST_BIN := $(MO)/test-malign
TEST_SRC := $(M)/test/malign.c

all: modules test_bin

modules:
	$(MAKE) -C $(KDIR) M=$(M) MO=$(MO) modules

modules_install:
	$(MAKE) -C $(KDIR) M=$(M) MO=$(MO) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(M) MO=$(MO) clean

help:
	$(MAKE) -C $(KDIR) M=$(M) MO=$(MO) help

$(TEST_BIN): $(TEST_SRC)
	$(CC) $< -o $@

test_bin: $(TEST_BIN)

test: test_bin
	$(TEST_BIN)

.PHONY: all modules modules_install clean help test_bin test
