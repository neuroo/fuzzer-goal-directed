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

if [ ! -d ./restbed ]; then
  echo "Pulling Restbed..."
  git clone --recursive https://github.com/corvusoft/restbed.git &> /dev/null
else
  echo "Restbed already pulled"
fi

mkdir -p ./build/restbed
mkdir -p ./dist/restbed
RESTBED_SRC_DIR=$(read_link ./restbed)
RESTBED_BUILD_DIR=$(read_link ./build/restbed)
RESTBED_DIST_DIR=$(read_link ./dist/restbed)

cd $RESTBED_BUILD_DIR
cmake -DBUILD_TESTS=NO -DBUILD_EXAMPLES=NO -DBUILD_SSL=NO -DBUILD_SHARED=YES -DCMAKE_INSTALL_PREFIX=$RESTBED_DIST_DIR $RESTBED_SRC_DIR
make clean
make -j 4 install
cd ..
