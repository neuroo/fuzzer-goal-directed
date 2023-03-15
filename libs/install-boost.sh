#!/usr/bin/env bash

command -v git >/dev/null 2>&1 || { echo >&2 "'git' is required but not installed. Aborting."; exit 1; }
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

if [ ! -d ./boost ]; then
  echo "Pulling Boost..."
  git clone --recursive https://github.com/boostorg/boost.git &> /dev/null
  cd boost
  git submodule init &> /dev/null
  git submodule update &> /dev/null
  cd ..
else
  echo "Boost already pulled"
fi

mkdir -p ./build/boost
mkdir -p ./dist/boost
BOOST_BUILD_DIR=$(read_link ./build/boost)
BOOST_DIST_DIR=$(read_link ./dist/boost)

cd ./boost

# XXX for release, we can limit what we need for built libs
./bootstrap.sh --prefix=$BOOST_DIST_DIR
./b2 install --build-dir=$BOOST_BUILD_DIR --prefix=$BOOST_DIST_DIR

wget http://www.highscore.de/boost/process.zip
unzip process.zip
cp -rf ./boost/process.hpp $BOOST_DIST_DIR/include/boost
mkdir -p $BOOST_DIST_DIR/include/boost/process
cp -rf ./boost/process/* $BOOST_DIST_DIR/include/boost/process

# For some reason, some libs don't get copied... let's copy the header only lib
cp -rf ./libs/flyweight/include/boost/* $BOOST_DIST_DIR/include/boost
cp -rf ./libs/interprocess/include/boost/* $BOOST_DIST_DIR/include/boost
cp -rf ./libs/lockfree/include/boost/* $BOOST_DIST_DIR/include/boost
cp -rf ./libs/heap/include/boost/* $BOOST_DIST_DIR/include/boost

cd ..
