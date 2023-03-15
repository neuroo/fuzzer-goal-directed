include ../rules/common.mk

LOC_BUILD_DIR=../$(BUILD_DIR)/clang-instrument
LOC_DIST_DIR=../$(DIST_DIR)/clang-instrument


SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/$(LIB_CLANG_INSTRUMENT_TARGET)

COMMON_LDFLAG=../$(DIST_DIR)/common/$(LIB_COMMON)

ifeq ($(OS),Darwin)
  LDFLAGS=$(LDFLAGS) -Wl,-undefined,dynamic_lookup
endif


.PHONY: clean

all: prepare clean_exec copy_libs $(OBJS) $(EXEC)


$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(INC) $< -o $@ $(LLVM_CXXFLAGS)


$(EXEC): prepare clean_exec copy_libs $(OBJS)
	$(CXX) -o $(EXEC) $(OFLAGS) \
	$(shell find $(LOC_BUILD_DIR) -type f -name '*.o') \
	$(COMMON_LDFLAG) $(LLVM_LDFLAGS) $(LDFLAGS_SHARED)

copy_libs: prepare clean_exec
	@cp -f $(LLVM_DIR)/lib/*.dylib $(LOC_DIST_DIR)

tests:
	@for dir in $(TESTS_DIR); do ( $(MAKE) -C $$dir --print-directory -j 1 tests ); done


prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)


clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)
	@rm -rf $(LOC_DIST_DIR)

clean_exec: prepare
	@rm -f $(EXEC)
