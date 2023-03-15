include ../rules/common.mk

SUBDIRS=pin-intercept-spawn


all: compile

compile:
	@for dir in $(SUBDIRS); do ( $(MAKE) -C $$dir --print-directory -j 1 ); done
