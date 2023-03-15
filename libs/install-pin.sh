#!/usr/bin/env bash

command -v wget >/dev/null 2>&1 || { echo >&2 "'wget' is required but not installed. Aborting."; exit 1; }

function read_link() {
  local path=$1
  if [ -d $path ] ; then
    local abspath=$(cd $path; pwd)
  else
    local prefix=$(cd $(dirname -- $path) ; pwd)
    local suffix=$(basename $path)
    local abspath="$prefix/$suffix"
  fi
  echo $abspath
}

OS=`uname -s`
if [[ "$OS" == "Windows" ]]; then
  echo "Only supporting OSX/Linux for now..."
  exit
fi

FILENAME="pin-2.14-71313"

URL=""
if [[ "$OS" == "Linux" ]]; then
  URL=$LINUX_URL
  EXTRACTED_PATH="$FILENAME-gcc.4.4.7-linux"
  FILENAME="$FILENAME-gcc.4.4.7-linux.tar.gz"
fi

if [[ "$OS" == "Darwin" ]]; then
  URL=$OSX_URL
  EXTRACTED_PATH="$FILENAME-clang.5.1-mac"
  FILENAME="$FILENAME-clang.5.1-mac.tar.gz"
fi

PIN_DL_URL="http://software.intel.com/sites/landingpage/pintool/downloads/$FILENAME"

mkdir -p ./build/pin
mkdir -p ./dist/pin
mkdir -p ./dist/pin/include
mkdir -p ./dist/pin/lib
mkdir -p ./dist/pin/bin
mkdir -p ./dist/pin/bin/ia32/bin
mkdir -p ./dist/pin/bin/intel64/bin
PIN_SRC_DIR=$(read_link ./pin)
PIN_BUILD_DIR=$(read_link ./build/pin)
PIN_DIST_DIR=$(read_link ./dist/pin)

if [ ! -f $PIN_BUILD_DIR/$FILENAME ]; then
  echo "downloading pin..."
  wget $PIN_DL_URL --directory-prefix $PIN_BUILD_DIR &> /dev/null
fi

if [ ! -d $PIN_BUILD_DIR/$EXTRACTED_PATH ]; then
  cd $PIN_BUILD_DIR
  tar -zxf $FILENAME &> /dev/null
  cd ../..
fi

# copy the headers
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/source/include/pin $PIN_DIST_DIR/include

mkdir -p $PIN_DIST_DIR/include/components
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/extras/components/include/* $PIN_DIST_DIR/include/components

mkdir -p $PIN_DIST_DIR/include/xed-intel64
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/extras/xed-intel64/include/* $PIN_DIST_DIR/include/xed-intel64

# copy the libs
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/intel64/lib/*.a $PIN_DIST_DIR/lib
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/intel64/lib-ext/*.a $PIN_DIST_DIR/lib
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/extras/components/lib/intel64/*.a $PIN_DIST_DIR/lib
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/extras/xed-intel64/lib/*.a $PIN_DIST_DIR/lib

# copy the binaries
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/pin $PIN_DIST_DIR/bin

cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/intel64/bin/* $PIN_DIST_DIR/bin/intel64/bin/
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/extras/xed-intel64/bin/* $PIN_DIST_DIR/bin/intel64/bin/
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/ia32/bin/* $PIN_DIST_DIR/bin/ia32/bin/
cp -rf $PIN_BUILD_DIR/$EXTRACTED_PATH/extras/xed-ia32/bin/* $PIN_DIST_DIR/bin/ia32/bin/
