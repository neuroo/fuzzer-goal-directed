include ../../../rules/common.mk

FUZZER_BUILD_DIR=../$(BUILD_DIR)/fuzzer
INPUT_SOURCES=$(wildcard tests_*.cpp)
OUTPUT_SOURCES=$(patsubst tests_%.cpp, %.bin, $(INPUT_SOURCES))

CODE_PATH=-I../../../fuzzer

all: clean $(OUTPUT_SOURCES) run_tests clean

%.bin: tests_%.cpp
	$(CXX) $(CXXFLAGS) $(INC) $(CODE_PATH) $(shell find $(LOC_BUILD_DIR) -type f -name '*.o') $< -o $@ $(OFLAGS)

run_tests:
	@for unit_test in $(wildcard *.bin); do ( ./$$unit_test ); done

clean:
	@rm -f *.bin
	@rm -f *.log
	@rm -rf *.bin.dSYM
