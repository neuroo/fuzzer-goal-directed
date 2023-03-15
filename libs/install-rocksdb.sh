#!/usr/bin/env bash

command -v git >/dev/null 2>&1 || { echo >&2 "'git' is required but not installed. Aborting."; exit 1; }
command -v make >/dev/null 2>&1 || { echo >&2 "'cmake' is required but not installed. Aborting."; exit 1; }

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

if [ ! -d ./rocksdb ]; then
  echo "Pulling rocksdb..."
  git clone https://github.com/facebook/rocksdb.git &> /dev/null
else
  echo "rocksdb already pulled"
fi

mkdir -p ./build/rocksdb
mkdir -p ./dist/rocksdb
mkdir -p ./dist/rocksdb/include
mkdir -p ./dist/rocksdb/lib
ROCKSDB_SRC_DIR=$(read_link ./rocksdb)
ROCKSDB_BUILD_DIR=$(read_link ./build/rocksdb)
ROCKSDB_DIST_DIR=$(read_link ./dist/rocksdb)

cp -rf $ROCKSDB_SRC_DIR/* $ROCKSDB_BUILD_DIR/
cd $ROCKSDB_BUILD_DIR
make static_lib -j 6

cp -rf include/* $ROCKSDB_DIST_DIR/include/
cp -rf out-static/*.a  $ROCKSDB_DIST_DIR/lib/

cd ..
