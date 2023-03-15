include ../../rules/common.mk

LOC_BUILD_DIR=../../$(BUILD_DIR)/smoke-shm
LOC_DIST_DIR=../../$(DIST_DIR)/smoke-shm

SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/smoke-shm

SHARED_DATA_LIB=../../$(DIST_DIR)/shared-data/$(LIB_SHARED_DATA)

.PHONY: clean

all: prepare clean_exec $(OBJS) $(EXEC)


$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(INC) -I../.. $< -o $@


$(EXEC): prepare clean_exec $(OBJS)
	$(CXX) -o $(EXEC) $(shell find $(LOC_BUILD_DIR) -type f -name '*.o') $(SHARED_DATA_LIB)


prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)


clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)
	@rm -rf $(LOC_DIST_DIR)


clean_exec: prepare
	@rm -f $(EXEC)
