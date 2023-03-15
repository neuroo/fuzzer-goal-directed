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

if [ ! -d ./libbf ]; then
  echo "Pulling libbf..."
  git clone https://github.com/mavam/libbf.git &> /dev/null
else
  echo "libbf already pulled"
fi

mkdir -p ./build/libbf
mkdir -p ./dist/libbf
LIBBF_SRC_DIR=$(read_link ./libbf)
LIBBF_BUILD_DIR=$(read_link ./build/libbf)
LIBBF_DIST_DIR=$(read_link ./dist/libbf)
BOOST_DIST_DIR=$(read_link ./dist/boost)

cd $LIBBF_SRC_DIR
./configure --builddir=$LIBBF_BUILD_DIR --prefix=$LIBBF_DIST_DIR --with-boost=$BOOST_DIST_DIR
make clean &> /dev/null
make -j 4 install &> /dev/null
cd ..
