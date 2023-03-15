include ../../../rules/common.mk

INPUT_SOURCES=$(wildcard input_*.cpp)
OUTPUT_SOURCES=$(patsubst input_%, output_%, $(INPUT_SOURCES))

INSTRUMENTER_PATH=../../../dist/clang-instrument
INSTRUMENTER_EXEC=$(INSTRUMENTER_PATH)/$(LIB_CLANG_INSTRUMENT_TARGET)

RUNTIME_EXEC=../../../dist/runtime/libinstr-runtime.a


all: clean $(OUTPUT_SOURCES)

output_%.cpp: input_%.cpp
	$(CXX) -cc1 -load $(INSTRUMENTER_EXEC) -plugin instrument $< -o $@ $(CXX_INCLUDE) -std=c++11 -stdlib=libc++ -fcxx-exceptions
	$(CXX) -std=c++11 -stdlib=libc++ $(RUNTIME_EXEC) $@ $(LDFLAGS_APPLE_FOUNDATION) -o $@.bin

clean:
	@rm -f *.xxx
	@rm -f output_*
	@rm -f instrument.log
