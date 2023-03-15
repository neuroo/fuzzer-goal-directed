include ../../rules/common.mk

LOC_BUILD_DIR=../../$(BUILD_DIR)/new-trace-sharing
LOC_DIST_DIR=../../$(DIST_DIR)/new-trace-sharing

SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/new-trace-sharing

SHARED_DATA_LIB=../../$(DIST_DIR)/shared-data/$(LIB_SHARED_DATA)
LIBZMQ_DATA_LIB=$(ZMQ_DIR)/lib/libzmq-static.a

.PHONY: clean

all: prepare clean_exec copy_dylibs $(OBJS)  $(EXEC)


$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(INC) -I../.. -I../../include $< -o $@


$(EXEC): prepare clean_exec $(OBJS)
	$(CXX) -o $(EXEC) $(shell find $(LOC_BUILD_DIR) -type f -name '*.o') \
         $(SHARED_DATA_LIB) $(LIBZMQ_DATA_LIB) \
         -L$(BOOST_DIR)/lib -lboost_program_options -lboost_thread \
         -lboost_system -lboost_filesystem \
         -lboost_timer -lboost_chrono -lboost_regex

copy_dylibs: prepare clean_exec
	@cp -f $(BOOST_DIR)/lib/libboost_program_options.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_thread.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_system.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_filesystem.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_regex.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_timer.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_chrono.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)
	@cp -f $(BOOST_DIR)/lib/libboost_atomic.$(DYNAMIC_LIB_EXT) $(LOC_DIST_DIR)

prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)


clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)
	@rm -rf $(LOC_DIST_DIR)


clean_exec: prepare
	@rm -f $(EXEC)
