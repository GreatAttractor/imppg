# syntax=docker/dockerfile:1
FROM ubuntu:24.04
RUN \
    apt update && \
    DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt install -y tzdata keyboard-configuration && \
    apt install -y git cmake build-essential libboost-dev libboost-test-dev libwxgtk3.2-dev libglew-dev pkg-config libccfits-dev libfreeimage-dev liblua5.3-dev
COPY build_imppg.sh /
RUN chmod u+x /build_imppg.sh
