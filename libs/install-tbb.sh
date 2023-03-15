#!/usr/bin/env bash

command -v git >/dev/null 2>&1 || { echo >&2 "'git' is required but not installed. Aborting."; exit 1; }

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

if [ ! -d ./tbb ]; then
  echo "Pulling tbb..."
  git clone --recursive https://github.com/intel-tbb/intel-tbb.git tbb &> /dev/null
else
  echo "tbb already pulled"
fi

mkdir -p ./build/tbb
mkdir -p ./dist/tbb
TBB_SRC_DIR=$(read_link ./tbb)
TBB_BUILD_DIR=$(read_link ./build/tbb)
TBB_DIST_DIR=$(read_link ./dist/tbb)

cd $TBB_SRC_DIR
make clean &> /dev/null
make compiler=clang stdlib=libc++ -j 4 &> /dev/null

# Manually copy headers and libs in the right location
cp -rf ./include $TBB_DIST_DIR
mkdir -p $TBB_DIST_DIR/lib
cp -f ./build/*_release/*.dylib $TBB_DIST_DIR/lib  &> /dev/null
cp -f ./build/*_release/*.dll $TBB_DIST_DIR/lib  &> /dev/null
cp -f ./build/*_release/*.so $TBB_DIST_DIR/lib  &> /dev/null
cp -f ./build/*_release/*.a $TBB_DIST_DIR/lib  &> /dev/null
cd ..


