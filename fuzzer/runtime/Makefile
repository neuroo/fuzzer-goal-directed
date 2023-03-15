include ../rules/common.mk

LOC_BUILD_DIR=../$(BUILD_DIR)/runtime
LOC_DIST_DIR=../$(DIST_DIR)/runtime
EXP_MERGED_ARCHIVES=../$(DIST_DIR)/runtime/merged

SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp, $(LOC_BUILD_DIR)/%.o, $(SRCS))
EXEC=$(LOC_DIST_DIR)/$(LIB_RUNTIME)

SHARED_DATA_LIB=../$(DIST_DIR)/shared-data/$(LIB_SHARED_DATA)

BREAKPAD_LIB=$(BREAKPAD_DIR)/lib/libbreakpad.a \
             $(BREAKPAD_DIR)/lib/libbreakpad_common.a \
             $(BREAKPAD_DIR)/lib/libdisasm.a


.PHONY: clean

all: prepare clean_exec expand_archives $(OBJS)  $(EXEC)

$(LOC_BUILD_DIR)/%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(INC) $< -o $@


$(EXEC):
	$(AR) rcs $(EXEC) $(shell find $(LOC_BUILD_DIR) -type f -name '*.o') \
	$(shell find $(EXP_MERGED_ARCHIVES)/libshared-data -type f -name '*.o') \
	$(shell find $(EXP_MERGED_ARCHIVES)/libbreakpad -type f -name '*.o') \
	$(shell find $(EXP_MERGED_ARCHIVES)/libbreakpad_common -type f -name '*.o') \
	$(shell find $(EXP_MERGED_ARCHIVES)/libdisasm -type f -name '*.o')


expand_archives:
	@cp -f $(SHARED_DATA_LIB) $(EXP_MERGED_ARCHIVES)/libshared-data
	@cp -f $(BREAKPAD_DIR)/lib/libbreakpad.a $(EXP_MERGED_ARCHIVES)/libbreakpad
	@cp -f $(BREAKPAD_DIR)/lib/libbreakpad_common.a $(EXP_MERGED_ARCHIVES)/libbreakpad_common
	@cp -f $(BREAKPAD_DIR)/lib/libdisasm.a $(EXP_MERGED_ARCHIVES)/libdisasm

	@cd $(EXP_MERGED_ARCHIVES)/libshared-data && $(GNU_AR) -x $(LIB_SHARED_DATA)
	@cd $(EXP_MERGED_ARCHIVES)/libbreakpad && $(GNU_AR) -x libbreakpad.a
	@cd $(EXP_MERGED_ARCHIVES)/libbreakpad_common && $(GNU_AR) -x libbreakpad_common.a
	@cd $(EXP_MERGED_ARCHIVES)/libdisasm && $(GNU_AR) -x libdisasm.a

	@cd $(EXP_MERGED_ARCHIVES)/libshared-data && chmod +rw __.*
	@cd $(EXP_MERGED_ARCHIVES)/libbreakpad && chmod +rw __.*
	@cd $(EXP_MERGED_ARCHIVES)/libbreakpad_common && chmod +rw __.*
	@cd $(EXP_MERGED_ARCHIVES)/libdisasm && chmod +rw __.*


prepare:
	@mkdir -p $(LOC_BUILD_DIR)
	@mkdir -p $(LOC_DIST_DIR)
	@mkdir -p $(EXP_MERGED_ARCHIVES)/libshared-data
	@mkdir -p $(EXP_MERGED_ARCHIVES)/libbreakpad
	@mkdir -p $(EXP_MERGED_ARCHIVES)/libbreakpad_common
	@mkdir -p $(EXP_MERGED_ARCHIVES)/libdisasm

clean:
	@rm -f $(OBJS)
	@rm -rf $(LOC_BUILD_DIR)
	@rm -rf $(LOC_DIST_DIR)
	@rm -rf $(EXP_MERGED_ARCHIVES)

clean_exec:
	@rm -f $(EXEC)
