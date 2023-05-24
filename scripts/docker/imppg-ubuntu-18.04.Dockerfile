# syntax=docker/dockerfile:1
FROM ubuntu:18.04
RUN \
    apt update && \
    apt install -y git cmake build-essential libboost-dev libboost-test-dev libwxgtk3.0-gtk3-dev libglew-dev pkg-config libccfits-dev libfreeimage-dev liblua5.3-dev && \
    apt install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt install -y gcc-8 g++-8 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 80 --slave /usr/bin/g++ g++ /usr/bin/g++-8 --slave /usr/bin/gcov gcov /usr/bin/gcov-8
COPY build_imppg.sh /
RUN chmod u+x /build_imppg.sh
