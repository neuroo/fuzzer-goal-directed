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

if [ ! -d ./jsoncpp ]; then
  echo "Pulling jsoncpp..."
  git clone https://github.com/open-source-parsers/jsoncpp.git &> /dev/null
else
  echo "jsoncpp already pulled"
fi

mkdir -p ./build/jsoncpp
mkdir -p ./dist/jsoncpp
JSONCPP_SRC_DIR=$(read_link ./jsoncpp)
JSONCPP_BUILD_DIR=$(read_link ./build/jsoncpp)
JSONCPP_DIST_DIR=$(read_link ./dist/jsoncpp)


cd $JSONCPP_BUILD_DIR
make clean
cmake -DCMAKE_BUILD_TYPE=release -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF -DINCLUDE_INSTALL_DIR=$JSONCPP_DIST_DIR/include -DARCHIVE_INSTALL_DIR=$JSONCPP_DIST_DIR/lib -G "Unix Makefiles" $JSONCPP_SRC_DIR
make -j 4
make install
cd ..
