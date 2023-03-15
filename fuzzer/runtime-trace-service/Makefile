include ../rules/common.mk

LOC_BUILD_DIR=../$(BUILD_DIR)/runtime-trace-service
LOC_DIST_DIR=../$(DIST_DIR)/runtime-trace-service


SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/$(LIB_RUNTIME_TRACE_SERVICE_TARGET)

COMMON_LDFLAG=../$(DIST_DIR)/common/$(LIB_COMMON)
SHARED_DATA_LIB=../$(DIST_DIR)/shared-data/$(LIB_SHARED_DATA)


ifeq ($(OS),Darwin)
  LDFLAGS=$(LDFLAGS) -Wl,-undefined,dynamic_lookup
endif


.PHONY: clean

all: prepare clean_exec copy_dylibs copy_scripts $(OBJS) $(EXEC)


$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(INC) $(RUNTIME_TRACE_CXX_FLAGS) $< -o $@ $(LLVM_CXXFLAGS)


$(EXEC):
	$(CXX) -o $(EXEC) $(OFLAGS) \
	$(shell find $(LOC_BUILD_DIR) -type f -name '*.o') \
	$(COMMON_LDFLAG) $(SHARED_DATA_LIB) $(FUZZER_LD_FLAGS) $(RUNTIME_TRACE_LD_FLAGS)


tests:
	@for dir in $(TESTS_DIR); do ( $(MAKE) -C $$dir --print-directory -j 1 tests ); done

copy_dylibs:
	@cp -f $(BOOST_DIR)/lib/libboost_program_options.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_thread.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_system.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_filesystem.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_graph.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_regex.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_atomic.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	#
	@cp -f $(RESTBED_DIR)/library/librestbed.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)

copy_scripts:
	@cp -f ./test_scripts/*.sh $(LOC_DIST_DIR)
	@chmod +x $(LOC_DIST_DIR)/*.sh

prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)


clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)
	@rm -rf $(LOC_DIST_DIR)

clean_exec:
	@rm -f $(EXEC)
