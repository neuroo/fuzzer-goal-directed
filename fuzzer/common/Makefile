include ../rules/common.mk

LOC_BUILD_DIR=../$(BUILD_DIR)/common
LOC_DIST_DIR=../$(DIST_DIR)/common

SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/$(LIB_COMMON)


.PHONY: clean

all: prepare clean_exec $(OBJS) $(EXEC)

$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(INC) $< -o $@


$(EXEC):
	$(AR) $(ARFLAGS) $(EXEC) $(shell find $(LOC_BUILD_DIR) -type f -name '*.o')


prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)

clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)
	@rm -rf $(LOC_DIST_DIR)

clean_exec:
	@rm -f $(EXEC)
