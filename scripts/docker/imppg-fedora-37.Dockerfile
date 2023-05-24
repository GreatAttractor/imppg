# syntax=docker/dockerfile:1
FROM fedora:37
RUN dnf install -y git cmake g++ pkgconf-pkg-config boost-devel wxGTK3-devel cfitsio-devel glew-devel lua-devel freeimage-devel rpm-build
COPY build_imppg.sh /
RUN chmod u+x /build_imppg.sh
