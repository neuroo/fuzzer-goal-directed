#!/usr/bin/env bash

command -v svn >/dev/null 2>&1 || { echo >&2 "'svn' is required but not installed. Aborting."; exit 1; }


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


if [ ! -d ./llvm ]; then
  echo "Pulling LLVM..."
  svn co http://llvm.org/svn/llvm-project/llvm/trunk llvm &>/dev/null

  echo "Pulling CLANG..."
  cd llvm/tools
  svn co http://llvm.org/svn/llvm-project/cfe/trunk clang &>/dev/null
  cd ../..

  cd llvm/tools/clang/tools
  svn co http://llvm.org/svn/llvm-project/clang-tools-extra/trunk extra &>/dev/null
  cd ../../../..

  echo "Pulling CXX/Runtime..."
  cd llvm/projects
  svn co http://llvm.org/svn/llvm-project/compiler-rt/trunk compiler-rt &>/dev/null
  svn co http://llvm.org/svn/llvm-project/libcxx/trunk libcxx &>/dev/null
  svn co http://llvm.org/svn/llvm-project/libcxxabi/trunk libcxxabi &>/dev/null
  svn co http://llvm.org/svn/llvm-project/openmp/trunk openmp &>/dev/null
  cd ../..
else
  echo "LLVM already pulled."
fi


mkdir -p ./build/llvm
mkdir -p ./dist/llvm
LLVM_BUILD_DIR=$(read_link ./build/llvm)
LLVM_DIST_DIR=$(read_link ./dist/llvm)

cd $LLVM_BUILD_DIR
CC=clang CXX=clang++ cmake -DLLVM_ENABLE_RTTI=1 -DLLVM_ENABLE_EH=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$LLVM_DIST_DIR "Unix Makefiles" ../../llvm
make -j 12
make install
cd ../..
