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

if [ ! -d ./cpp.react ]; then
  echo "Pulling cpp.react..."
  git clone https://github.com/schlangster/cpp.react.git &> /dev/null
else
  echo "cpp.react already pulled"
fi

mkdir -p ./build/cpp.react
mkdir -p ./dist/cpp.react
REACT_SRC_DIR=$(read_link ./cpp.react)
REACT_BUILD_DIR=$(read_link ./build/cpp.react)
REACT_DIST_DIR=$(read_link ./dist/cpp.react)

cd $REACT_BUILD_DIR
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=$REACT_DIST_DIR $REACT_SRC_DIR
make clean
make -j 4
make install
cd ..



