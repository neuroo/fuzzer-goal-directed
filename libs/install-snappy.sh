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

mkdir -p ./build/snappy
mkdir -p ./dist/snappy
SNAPPY_BUILD_DIR=$(read_link ./build/snappy)
SNAPPY_DIST_DIR=$(read_link ./dist/snappy)

SNAPPY_URL="https://github.com/google/snappy/releases/download/1.1.3/snappy-1.1.3.tar.gz"
SNAPPY_FILENAME="snappy-1.1.3.tar.gz"
EXTRACTED_PATH="snappy-1.1.3"

if [ ! -f $SNAPPY_BUILD_DIR/$SNAPPY_FILENAME ]; then
  echo "Pulling snappy..."
  wget $SNAPPY_URL --directory-prefix $SNAPPY_BUILD_DIR &> /dev/null
fi

if [ ! -d $SNAPPY_BUILD_DIR/$EXTRACTED_PATH ]; then
  cd $SNAPPY_BUILD_DIR
  tar -jxf $SNAPPY_FILENAME
  cd ../..
fi

cd $SNAPPY_BUILD_DIR/$EXTRACTED_PATH
./configure --prefix=$SNAPPY_DIST_DIR
make -j 4
make install
cd ..
