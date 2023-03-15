include ../rules/common.mk

LOC_BUILD_DIR=../$(BUILD_DIR)/fuzzer
LOC_DIST_DIR=../$(DIST_DIR)/fuzzer


SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/$(FUZZER_TARGET)

COMMON_LDFLAG=../$(DIST_DIR)/common/$(LIB_COMMON)
SHARED_DATA_LIB=../$(DIST_DIR)/shared-data/$(LIB_SHARED_DATA)

ifeq ($(OS),Darwin)
  LDFLAGS=$(LDFLAGS) -Wl,-undefined,dynamic_lookup
endif


BREAKPAD_LIB=$(BREAKPAD_DIR)/lib/libbreakpad.a \
             $(BREAKPAD_DIR)/lib/libbreakpad_common.a \
             $(BREAKPAD_DIR)/lib/libbreakpad_reader.a \
             $(BREAKPAD_DIR)/lib/libdisasm.a

JSONCPP_LIB=$(JSONCPP_DIR)/lib/libjsoncpp.a

UI_DIR=ui

.PHONY: clean


all: prepare clean_exec copy_dylibs copy_scripts ui $(OBJS) $(EXEC)


$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(XSAN_CXXFLAGS) $(CXXFLAGS) $(INC) $< -o $@


$(EXEC): prepare clean_exec copy_dylibs copy_scripts $(OBJS)
	$(CXX) -o $(EXEC) $(XSAN_OFLAGS) $(OFLAGS) \
	$(shell find $(LOC_BUILD_DIR) -type f -name '*.o') \
	$(BREAKPAD_LIB) \
	$(JSONCPP_LIB) \
	$(COMMON_LDFLAG) $(SHARED_DATA_LIB) $(FUZZER_LD_FLAGS)


tests:
	@for dir in $(TESTS_DIR); do ( $(MAKE) -C $$dir --print-directory -j 1 tests ); done


copy_dylibs: prepare clean_exec copy_scripts
	@cp -f $(BOOST_DIR)/lib/libboost_program_options.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_thread.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_system.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_filesystem.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_graph.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_regex.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_timer.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_chrono.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_atomic.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(LIBBF_DIR)/lib/libbf.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(TBB_DIR)/lib/libtbb.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(TBB_DIR)/lib/libtbbmalloc.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BREAKPAD_DIR)/bin/dump_syms $(LOC_DIST_DIR)

ui:
	@for dir in $(UI_DIR); do ( $(MAKE) -C $$dir --print-directory -j 1 tests ); done

copy_scripts: prepare clean_exec
	@cp -f ./test_scripts/*.sh $(LOC_DIST_DIR)
	@chmod +x $(LOC_DIST_DIR)/*.sh


prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)


clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)
	@rm -rf $(LOC_DIST_DIR)


clean_exec: prepare
	@rm -f $(EXEC)
