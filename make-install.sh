#!/usr/bin/env bash
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

INSTALL_DIR=$(read_link `pwd`/../fuzzer-install)
echo "Installing into $INSTALL_DIR"

mkdir -p $INSTALL_DIR &> /dev/null
mkdir -p $INSTALL_DIR/utils &> /dev/null

# 1. Copy all of our generated programs into `fuzzer`
cp -f ./dist/clang-instrument/libclang-instrument.* $INSTALL_DIR &> /dev/null

cp -f ./dist/fuzzer/coverage-fuzz $INSTALL_DIR &> /dev/null
cp -f ./dist/fuzzer/*.dylib $INSTALL_DIR &> /dev/null
cp -f ./dist/fuzzer/*.dll $INSTALL_DIR &> /dev/null
cp -f ./dist/fuzzer/*.so $INSTALL_DIR &> /dev/null
cp -f ./dist/runtime/libinstr-runtime.a $INSTALL_DIR &> /dev/null

cp -f ./dist/runtime-trace-service/* $INSTALL_DIR &> /dev/null
cp -f ./dist/pin-intercept-spawn/intercept-spawn.* $INSTALL_DIR &> /dev/null
cp -f ./utils/build-wrap* $INSTALL_DIR &> /dev/null

# 2. Copy system dependencies we'll need llvm and pin
if [ ! -d $INSTALL_DIR/utils/llvm/ ]; then
  mkdir -p $INSTALL_DIR/utils/llvm/ &> /dev/null
  cp -rf ../libs/dist/llvm/* $INSTALL_DIR/utils/llvm/ &> /dev/null
fi

if [ ! -d $INSTALL_DIR/utils/pin/ ]; then
  mkdir -p $INSTALL_DIR/utils/pin/ &> /dev/null
  cp -rf ../libs/dist/pin/* $INSTALL_DIR/utils/pin/ &> /dev/null
fi
