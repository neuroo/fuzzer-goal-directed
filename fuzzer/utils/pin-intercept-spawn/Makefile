include ../../rules/common.mk

LOC_BUILD_DIR=../../$(BUILD_DIR)/pin-intercept-spawn
LOC_DIST_DIR=../../$(DIST_DIR)/pin-intercept-spawn


SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/$(PIN_INTERCEPT_SPAWN)

COMMON_LDFLAG=../$(DIST_DIR)/common/$(LIB_COMMON)

ifeq ($(OS),Darwin)
  LDFLAGS=$(LDFLAGS) -Wl,-undefined,dynamic_lookup -pie
endif

CXX=$(PIN_CXX)

.PHONY: clean

all: clean prepare clean_exec copy_runtime $(OBJS) $(EXEC)


$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(PIN_CXXFLAGS) $< -o $@


$(EXEC):
	$(CXX) -o $(EXEC) \
	$(shell find $(LOC_BUILD_DIR) -type f -name '*.o') \
	$(PIN_LD_FLAGS)

copy_runtime:
	@cp -rf $(PIN_DIR)/bin/* $(LOC_DIST_DIR)/

tests:
	@for dir in $(TESTS_DIR); do ( $(MAKE) -C $$dir --print-directory -j 1 tests ); done


prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)

clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)/*.o

clean_exec:
	@rm -f $(EXEC)
