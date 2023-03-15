#!/usr/bin/env bash

command -v git >/dev/null 2>&1 || { echo >&2 "'git' is required but not installed. Aborting."; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo >&2 "'cmake' is required but not installed. Aborting."; exit 1; }

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

if [ ! -d ./leveldb ]; then
  echo "Pulling leveldb..."
  git clone https://github.com/google/leveldb.git &> /dev/null
else
  echo "leveldb already pulled"
fi

mkdir -p ./build/leveldb
mkdir -p ./dist/leveldb
mkdir -p ./dist/leveldb/include
mkdir -p ./dist/leveldb/lib
LEVELDB_SRC_DIR=$(read_link ./leveldb)
LEVELDB_BUILD_DIR=$(read_link ./build/leveldb)
LEVELDB_DIST_DIR=$(read_link ./dist/leveldb)

cp -rf $LEVELDB_SRC_DIR/* $LEVELDB_BUILD_DIR/
cd $LEVELDB_BUILD_DIR
./build_detect_platform config_output $LEVELDB_DIST_DIR
make -j 6

cp -rf include/* $LEVELDB_DIST_DIR/include/
cp -rf out-static/*.a  $LEVELDB_DIST_DIR/lib/

cd ..
