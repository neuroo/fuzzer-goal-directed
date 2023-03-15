include ../../rules/common.mk

LOC_BUILD_DIR=../../$(BUILD_DIR)/smoke-process
LOC_DIST_DIR=../../$(DIST_DIR)/smoke-process

SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/smoke-process
SHARED_DATA_LIB=../../$(DIST_DIR)/shared-data/$(LIB_SHARED_DATA)


.PHONY: clean

all: prepare clean_exec copy_dylibs $(OBJS) $(EXEC)


$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(INC) -I../.. $< -o $@


$(EXEC):
	$(CXX) -o $(EXEC) $(shell find $(LOC_BUILD_DIR) -type f -name '*.o') $(SHARED_DATA_LIB) $(FUZZER_LD_FLAGS)

copy_dylibs:
	@cp -f $(BOOST_DIR)/lib/libboost_program_options.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_thread.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_system.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_filesystem.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_graph.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_regex.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)

prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)


clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)
	@rm -rf $(LOC_DIST_DIR)


clean_exec:
	@rm -f $(EXEC)
