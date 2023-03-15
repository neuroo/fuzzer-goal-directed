include rules/common.mk

SUBDIRS=shared-data common runtime clang-instrument fuzzer utils runtime-trace-service


all: compile

.PHONY: clean

compile: prepare
	@for dir in $(SUBDIRS); do ( $(MAKE) -C $$dir --print-directory -j 1 ); done

tests:
	@for dir in $(SUBDIRS); do ( $(MAKE) -C $$dir --print-directory -j 1 tests ); done

install:
	@../make-install.sh

clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(DIST_DIR)

prepare:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(DIST_DIR)
