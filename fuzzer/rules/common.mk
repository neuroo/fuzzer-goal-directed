ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
LLVM_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/llvm)
BOOST_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/boost)
BREAKPAD_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/breakpad)
RESTBED_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/restbed)
SEQAN_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/seqan)
PIN_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/pin)
JSONCPP_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/jsoncpp)
LEVELDB_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/leveldb)
SNAPPY_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/snappy)
LIBBF_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/libbf)
TBB_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/tbb)
ZMQ_DIR:=$(abspath $(ROOT_DIR)/../../libs/dist/libzmq)

BUILD_DIR=build
DIST_DIR=dist
TESTS_DIR=tests

OS=$(shell uname -s)

# That's for debugging the fuzzer only, not related to the SUT
USE_ASAN=$(ENV_USE_ASAN)
USE_TSAN=$(ENV_USE_TSAN)

LLVM_LOCAL_DIR:=$(LLVM_DIR)


CXX=$(LLVM_LOCAL_DIR)/bin/clang++
CC=$(LLVM_LOCAL_DIR)/bin/clang
AR=$(LLVM_LOCAL_DIR)/bin/llvm-ar
GNU_AR=ar
LLVM_CONFIG=$(LLVM_LOCAL_DIR)/bin/llvm-config
CLANG_TIDY=$(LLVM_LOCAL_DIR)/bin/clang-tidy

CLANG_TIDY_CONFIG=-checks=-*,clang-analyzer-*,clang-analyzer-alpha*,modernize-*

CXX_INCLUDE=-I$(shell $(LLVM_CONFIG) --includedir) \
            -I$(LLVM_DIR)/include/c++/v1 \
            -Iinclude -I../include -I. -I.. \
            -I$(BOOST_DIR)/include \
            -I$(BREAKPAD_DIR)/include \
            -I$(SEQAN_DIR)/include \
            -I$(JSONCPP_DIR)/include \
            -I$(LEVELDB_DIR)/include \
            -I$(SNAPPY_DIR)/include \
            -I$(LIBBF_DIR)/include \
            -I$(TBB_DIR)/include \
            -I$(ZMQ_DIR)/include
CXX_LIB=-L$(shell $(LLVM_CONFIG) --libdir)

INC=-I.
DEBUG=-g
ifeq ($(ENV_NO_DEBUG), 1)
	DEBUG=
endif

ARCH=-arch x86_64
OPT_LEVEL=-O2
ifeq ($(USE_ASAN), 1)
	OPT_LEVEL=-O1
endif

OFLAGS=$(DEBUG) -stdlib=libc++ -lc++ -lc++abi $(ARCH)
LDFLAGS_SHARED=-shared $(CXX_LIB) $(ARCH) -lz -lpthread -lm -ldl -lc++ -lc++abi -m64
CXXFLAGS=$(DEBUG) -Wall -Wno-unknown-pragmas $(ARCH) $(OPT_LEVEL) -fPIC \
         -std=c++11 -fno-stack-protector $(CXX_INCLUDE) \
         -stdlib=libc++ -fvisibility=hidden -fvisibility-inlines-hidden \
         -D_WEBSOCKETPP_CPP11_STL_
ARFLAGS=rcsu

LDFLAGS_APPLE_FOUNDATION=

#
# Lib with common definitions and impl.
#
LIB_COMMON=libinstr-common.a

#
# Runtime is platform independent
#
LIB_RUNTIME=libinstr-runtime.a

#
# Shared data is included by a fuzzer and the runtime. It allows
# to communicate via shared memory
#
LIB_SHARED_DATA=libshared-data.a

#
# Name of the fuzzer binary
#
FUZZER_TARGET=coverage-fuzz

# Library used by the fuzzer
FUZZER_LD_FLAGS=-L$(BOOST_DIR)/lib -lboost_program_options -lboost_thread \
                -lboost_system -lboost_filesystem -lboost_graph \
                -lboost_timer -lboost_chrono \
                $(LEVELDB_DIR)/lib/libleveldb.a $(LEVELDB_DIR)/lib/libmemenv.a \
                $(SNAPPY_DIR)/lib/libsnappy.a \
                -L$(LIBBF_DIR)/lib -lbf \
                -L$(TBB_DIR)/lib -ltbb -ltbbmalloc \
                $(ZMQ_DIR)/lib/libzmq-static.a

#
# Service to convert the runtime trace into an easy to consume format
#
LIB_RUNTIME_TRACE_SERVICE_TARGET=runtime-trace-service

RUNTIME_TRACE_CXX_FLAGS=-I$(RESTBED_DIR)/include

RUNTIME_TRACE_LD_FLAGS=-L$(RESTBED_DIR)/library -lrestbed

#
# Pin tool to intercept build calls
#
PIN_INCLUDE:=$(PIN_DIR)/include/pin
PIN_INCLUDE_GEN:=$(PIN_DIR)/include/pin/gen
PIN_INCLUDE_EXTRA:=$(PIN_DIR)/include/components
PIN_INCLUDE_EXTRA_XED:=$(PIN_DIR)/include/xed-intel64
PIN_LIB:=$(PIN_DIR)/lib
PIN_VER:=$(PIN_DIR)/include/pin/pintool.ver
PIN_SYMBOLS:=-w -Wl,-exported_symbols_list -Wl,$(PIN_DIR)/include/pin/pintool.exp


PIN_INTERCEPT_SPAWN=intercept-spawn

PIN_CXXFLAGS=-I$(PIN_INCLUDE) -I$(PIN_INCLUDE_GEN) -I$(PIN_INCLUDE_EXTRA) \
             -I$(PIN_INCLUDE_EXTRA_XED) -DTARGET_IA32E -DHOST_IA32E -fPIC \
             -DBIGARRAY_MULTIPLIER=1 \
             -stdlib=libstdc++ -std=c++11 -fPIC \
             -fno-stack-protector -fomit-frame-pointer -fno-strict-aliasing \
             -DFUND_TC_TARGETCPU=FUND_CPU_INTEL64 \
             -DFUND_TC_HOSTCPU=FUND_CPU_INTEL64

PIN_LD_FLAGS=-L$(PIN_LIB) \
             $(PIN_SYMBOLS) \
             -shared -lpin -lxed -lpindwarf

 PIN_CXX=c++

#
# libclang-instrument name and compilation flags depend on the target
#
DYNAMIC_LIB_EXT=dylib
LIB_CLANG_INSTRUMENT_TARGET=libclang-instrument.dylib
ifeq ($(OS), Windows)
	LIB_CLANG_INSTRUMENT_TARGET=clang-instrument.dll
	DYNAMIC_LIB_EXT=dll
	LIB_RUNTIME_TRACE_SERVICE_TARGET=runtime-trace-service.exe

  PIN_CXXFLAGS:=$(PIN_CXXFLAGS) -DTARGET_WINDOWS
endif

ifeq ($(OS), Linux)
	LIB_CLANG_INSTRUMENT_TARGET=libclang-instrument.so
	DYNAMIC_LIB_EXT=so
	PIN_CXXFLAGS:=$(PIN_CXXFLAGS) -DTARGET_LINUX
endif

ifeq ($(OS), Darwin)
	DYNAMIC_LIB_EXT=dylib
	OFLAGS := $(OFLAGS) -rdynamic $(ARCH) -rpath @executable_path
	LDFLAGS_SHARED := $(LDFLAGS_SHARED) -ledit -lcurses -mmacosx-version-min=10.10 $(ARCH) -rpath @executable_path
	CXXFLAGS := $(CXXFLAGS) -mmacosx-version-min=10.10
	LIB_CLANG_INSTRUMENT_TARGET=libclang-instrument.dylib
	ARFLAGS := $(ARFLAGS)
	LDFLAGS_APPLE_FOUNDATION := $(LDFLAGS_APPLE_FOUNDATION) -framework CoreFoundation -framework ApplicationServices -framework Foundation $(ARCH)
	FUZZER_LD_FLAGS := $(FUZZER_LD_FLAGS) $(LDFLAGS_APPLE_FOUNDATION)

	RUNTIME_TRACE_LD_FLAGS := $(RUNTIME_TRACE_LD_FLAGS) $(ARCH) -rpath @executable_path

  ifeq ($(USE_ASAN), 1)
  	XSAN_CXXFLAGS:=-fsanitize=address -fsanitize-recover=undefined,integer -fno-omit-frame-pointer -fno-optimize-sibling-calls
  	XSAN_OFLAGS:=-fsanitize=address
  endif

  ifeq ($(USE_TSAN), 1)
  	XSAN_CXXFLAGS:=-fsanitize=thread -fno-omit-frame-pointer -fno-optimize-sibling-calls
  	XSAN_OFLAGS:=-fsanitize=thread
  endif

  PIN_CXXFLAGS:=$(PIN_CXXFLAGS) -DTARGET_MAC \
                -DFUND_TC_TARGETOS=FUND_OS_MAC \
                -DFUND_TC_HOSTOS=FUND_OS_MAC

  # The min version is used to link to the correct c++ runtime pin was linked to
  PIN_LD_FLAGS:=$(PIN_LD_FLAGS) -mmacosx-version-min=10.8
endif

PIN_INTERCEPT_SPAWN:=$(PIN_INTERCEPT_SPAWN).$(DYNAMIC_LIB_EXT)

#
# Some testing stuff...
#
LIB_SMOKE_ANALYSIS=libsmoke-analysis.dylib
ifeq ($(OS), Windows)
	LIB_SMOKE_ANALYSIS=smoke-analysis.dll
endif

ifeq ($(OS), Linux)
	LIB_SMOKE_ANALYSIS=libsmoke-analysis.so
endif



CLANG_LIBS := \
	-lclangFrontend \
	-lclangSerialization \
	-lclangDriver \
	-lclangTooling \
	-lclangCodeGen \
	-lclangParse \
	-lclangSema \
	-lclangAnalysis \
	-lclangRewriteFrontend \
	-lclangRewrite \
	-lclangEdit \
	-lclangAST \
	-lclangLex \
	-lclangBasic

LLVM_CXX_FLAGS_EXC_RTTI=-I$(LLVM_DIR)/include -fPIC -fvisibility-inlines-hidden -Wall -W \
                        -Wno-unused-parameter -Wwrite-strings -Wcast-qual \
                        -Wno-missing-field-initializers -pedantic -Wno-long-long \
                        -Wno-maybe-uninitialized -Wno-comment -std=c++11 -ffunction-sections \
                        -fdata-sections -DNDEBUG -D_GNU_SOURCE \
                        -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS


#LLVM_CXXFLAGS=$(shell $(LLVM_CONFIG) --cxxflags) -I$(CXX_LIB)/clang
LLVM_CXXFLAGS=$(LLVM_CXX_FLAGS_EXC_RTTI) -I$(CXX_LIB)/clang -fexceptions
LLVM_LDFLAGS=$(shell $(LLVM_CONFIG) --libs --ldflags) $(CLANG_LIBS) $(ARCH) -m64




