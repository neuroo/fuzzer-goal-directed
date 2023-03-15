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

if [ ! -d ./libzmq ]; then
  echo "Pulling libzmq..."
  git clone https://github.com/zeromq/libzmq &> /dev/null
else
  echo "libzmq already pulled"
fi

mkdir -p ./build/libzmq
mkdir -p ./dist/libzmq
ZMQ_SRC_DIR=$(read_link ./libzmq)
ZMQ_BUILD_DIR=$(read_link ./build/libzmq)
ZMQ_DIST_DIR=$(read_link ./dist/libzmq)

cd $ZMQ_BUILD_DIR
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=$ZMQ_DIST_DIR $ZMQ_SRC_DIR &> /dev/null
make clean &> /dev/null
make -j 4 &> /dev/null
make install &> /dev/null
cd ..
