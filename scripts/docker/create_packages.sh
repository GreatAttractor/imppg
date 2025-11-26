#!/bin/bash

#
# This script uses Docker to automatically build and package ImPPG for several Linux distributions.
#
# The first run takes some time (packages need to be downloaded and installed under Docker).
# Subsequent runs only rebuild ImPPG.
#

set -e

# select package types to build (see possible distributions under `packaging`)

PKG_TYPES="\
    ubuntu-18.04 \
    ubuntu-20.04 \
    ubuntu-22.04 \
    ubuntu-24.04 \
    fedora-37 \
    fedora-38 \
    fedora-39 \
    fedora-40 \
    fedora-41 \
    fedora-42 \
    fedora-43
"

MAPPED_IMPPG_REPO=/imppg_src

if [ "$#" -ne 2 ]; then
    echo "expected arguments: ImPPG repository path, Git hash"
    exit 1
fi

REPO_DIR=$1
GIT_HASH=$2

for PKG_TYPE in $PKG_TYPES; do
    IMAGE=imppg-$PKG_TYPE
    docker build --file $IMAGE.Dockerfile . -t $IMAGE
    docker run \
        --volume `pwd`:/host:z \
        --volume $REPO_DIR:$MAPPED_IMPPG_REPO:z,ro \
        $IMAGE \
        /build_imppg.sh $MAPPED_IMPPG_REPO $GIT_HASH $PKG_TYPE
done
