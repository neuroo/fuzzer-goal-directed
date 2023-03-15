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

if [ ! -d ./seqan ]; then
  echo "Pulling SeqAn..."
  git clone --recursive https://github.com/seqan/seqan.git &> /dev/null
  cd ./seqan
  echo " [+] Checking out 9e1449d4ba7dbc65436f83cc148f6cda04dc0e55"
  git checkout 9e1449d4ba7dbc65436f83cc148f6cda04dc0e55 &> /dev/null
  echo " [+] Apply build patch"
  git am --signoff < ../seqan_build_no_doc.patch &> /dev/null
  cd ..
else
  echo "SeqAn already pulled"
fi

mkdir -p ./build/seqan
mkdir -p ./dist/seqan
SEQAN_SRC_DIR=$(read_link ./seqan)
SEQAN_BUILD_DIR=$(read_link ./build/seqan)
SEQAN_DIST_DIR=$(read_link ./dist/seqan)

cd $SEQAN_BUILD_DIR
cmake -DCMAKE_INSTALL_PREFIX=$SEQAN_DIST_DIR -DSEQAN_BUILD_SYSTEM=SEQAN_RELEASE_LIBRARY $SEQAN_SRC_DIR &> /dev/null
make clean &> /dev/null
make -j 4 install &> /dev/null
cd ..
