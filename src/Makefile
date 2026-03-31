KDIR ?= /lib/modules/$(shell uname -r)/build
export KBUILD_EXTMOD := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
include $(KDIR)/Makefile
