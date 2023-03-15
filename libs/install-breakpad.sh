#!/usr/bin/env bash

command -v git >/dev/null 2>&1 || { echo >&2 "'git' is required but not installed. Aborting."; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo >&2 "'cmake' is required but not installed. Aborting."; exit 1; }
command -v cpio >/dev/null 2>&1 || { echo >&2 "'cpio' is required but not installed. Aborting."; exit 1; }


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

if [ ! -d ./breakpad ]; then
  echo "Pulling breakpad..."
  git clone https://github.com/neuroo/breakpad.git &> /dev/null
else
  echo "Breakpad already pulled"
fi

mkdir -p ./build/breakpad
mkdir -p ./dist/breakpad
BREAKPAD_SRC_DIR=$(read_link ./breakpad)
BREAKPAD_BUILD_DIR=$(read_link ./build/breakpad)
BREAKPAD_DIST_DIR=$(read_link ./dist/breakpad)


cd $BREAKPAD_BUILD_DIR
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=$BREAKPAD_DIST_DIR $BREAKPAD_SRC_DIR/mendeley  &> /dev/null
make clean &> /dev/null
make -j 12  &> /dev/null
cd ../..


# Now, we need to manually copy everything as there is no install
mkdir -p $BREAKPAD_DIST_DIR/lib
mkdir -p $BREAKPAD_DIST_DIR/include
mkdir -p $BREAKPAD_DIST_DIR/bin

# Static libraries
cp $BREAKPAD_BUILD_DIR/*.a $BREAKPAD_DIST_DIR/lib
cp $BREAKPAD_BUILD_DIR/dump_syms $BREAKPAD_DIST_DIR/bin
cp $BREAKPAD_BUILD_DIR/minidump_stackwalk $BREAKPAD_DIST_DIR/bin

# Header files
cd $BREAKPAD_SRC_DIR/src
find . -name '*.h' | cpio -pdm $BREAKPAD_DIST_DIR/include &> /dev/null
cd ../..
