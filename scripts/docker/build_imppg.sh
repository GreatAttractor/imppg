#!/bin/bash
set -e

if [ "$#" -ne 3 ]; then
    echo "expected arguments: ImPPG repository URL, Git hash, package type"
    exit 1
fi

REPO_URL=$1
GIT_HASH=$2
PKG_TYPE=$3

git config --global --add safe.directory $REPO_URL/.git

git clone $REPO_URL imppg
cd imppg
git checkout $GIT_HASH
mkdir build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DIMPPG_PKG_TYPE=$PKG_TYPE ..
make -j 4
cpack
cp *.deb *.rpm /host || :  # TODO: make the copying more specific