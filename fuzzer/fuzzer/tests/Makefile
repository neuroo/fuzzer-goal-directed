include ../../rules/common.mk

TESTS=unit_tests

all: tests

tests:
	@for dir in $(TESTS); do ( $(MAKE) -C $$dir --print-directory -j 1 ); done
