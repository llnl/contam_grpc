# Use a base image that includes CUDA drivers
FROM fedora:latest
#FROM nvcr.io/nvidia/cuda:12.8.1-cudnn-devel-ubi9
#FROM quay.io/centos/centos:stream8

# Install necessary dependencies

#ENV ARCH=x86_64
RUN dnf -y install --skip-unavailable epel-release && dnf -y install fakeroot
#RUN subscription-manager repos --enable codeready-builder-for-rhel-9-$ARCH-rpms
#RUN subscription-manager repos --enable codeready-builder-for-rhel-9-x86_64-rpms
#RUN dnf -y install --skip-unavailable epel-release
#RUN dnf -y install fakeroot
RUN fakeroot dnf upgrade -y && fakeroot dnf update -y
RUN fakeroot dnf install -y wget hostname libevent
RUN fakeroot dnf install -y awk gawk
RUN fakeroot dnf install -y vim patch wget make cmake gzip git gcc-c++ python3 python3-pip python3-devel
RUN fakeroot dnf install -y glibc-locale-source
RUN fakeroot python3 -m pip install --upgrade pip
RUN fakeroot pip3 install --no-cache-dir cffi numpy pycparser
RUN fakeroot pip3 install --no-cache-dir grpcio grpcio-tools
RUN localedef --inputfile=en_US --charmap=UTF-8 en_US.UTF-8
ENV LANG=en_US.UTF-8


# Create shared volume mount point
RUN fakeroot mkdir -p /shared

# Download and build ExaEpi
WORKDIR /app
RUN fakeroot git clone -b v1.73.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
WORKDIR /app/grpc
RUN fakeroot git submodule update --init && fakeroot mkdir -p /app/grpc/cmake/build
WORKDIR /app/grpc/cmake/build
RUN fakeroot cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_STANDARD=17 \
      -DCMAKE_INSTALL_PREFIX=/app/grpc/install \
      -DgRPC_ABSL_PROVIDER=module     \
      -DgRPC_CARES_PROVIDER=module    \
      -DgRPC_PROTOBUF_PROVIDER=module \
      -DgRPC_RE2_PROVIDER=module      \
      -DgRPC_SSL_PROVIDER=module      \
      -DgRPC_ZLIB_PROVIDER=module \
      ../.. && fakeroot make -j 8 install && fakeroot make clean
ENV gRPC_DIR=/app/grpc/install
ENV PATH=${PATH}:/app/grpc/install/bin
ENV LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/app/grpc/install/lib64:/app/grpc/install/lib

RUN fakeroot git clone https://github.com/AMReX-Agent/ExaEpi.git
RUN fakeroot mkdir -p ExaEpi/build
WORKDIR /app/ExaEpi/build
#RUN fakeroot cmake -DCMAKE_INSTALL_PREFIX=`realpath ..`/install ..
#RUN fakeroot cmake -DAMReX_GPU_BACKEND=CUDA -DCMAKE_INSTALL_PREFIX=`realpath ..`/install ..
#RUN fakeroot cmake -DAMReX_GPU_BACKEND=HIP -DAMReX_AMD_ARCH=${AMD_ARCH} -DCMAKE_INSTALL_PREFIX=`realpath ..`/install ..
#RUN fakeroot make -j 8

# Set working directory
WORKDIR /app
COPY *.tar.gz CMakeLists.txt /app

# Copy and extract any *.tgz files
RUN for archive in *.tar.gz; do \
    if [ -f "$archive" ]; then \
        echo "Extracting $archive..."; \
        tar -xzf "$archive" --no-same-owner -C /app; \
    fi; \
    done

WORKDIR /app/runContamxDll_OneZoneWthCtm/contamx-artifacts/3.4.1.10
RUN tar zxvf contam-x-3.4.1.10-Linux-64bit-cxlibShared-debian-bookworm.tar.gz --no-same-owner
RUN tar zxvf contam-x-3.4.1.10-Linux-64bit-cxlibShared-debian-buster.tar.gz --no-same-owner
WORKDIR contam-x-3.4.1.10-Linux-64bit-cxlibShared/src-cxlib-examples/example-01
COPY CMakeLists.txt /app/runContamxDll_OneZoneWthCtm/contamx-artifacts/3.4.1.10/contam-x-3.4.1.10-Linux-64bit-cxlibShared/src-cxlib-examples/example-01
RUN mkdir build
WORKDIR build
RUN cmake .. && make -j 8
WORKDIR ../../example-02
# Create CMakeLists.txt based on the one under example-01
RUN cat ../example-01/CMakeLists.txt | sed -e 's/cxLibDriver_testOneZoneWthCtm/cxLibDriver_testWpcOneFloor/g' > CMakeLists.txt
# Patch the source to add a missing header
RUN mv cxLibDriver_testWpcOneFloor.c cxLibDriver_testWpcOneFloor.c.bak && awk  '{ if ($2 ~ /stdlib/) {printf("%s\n#include <string.h>\n", $0)} else print $0}' cxLibDriver_testWpcOneFloor.c.bak > cxLibDriver_testWpcOneFloor.c
RUN mkdir build
WORKDIR build
RUN cmake .. && make -j 8
WORKDIR /app
RUN fakeroot pip3 install cffi numpy pycparser contamxpy

COPY /data/contam-grpc.tgz /app
WORKDIR /app
RUN fakeroot tar zxvf contam-grpc.tgz --no-same-owner && mkdir -p contam-grpc/build
WORKDIR /app/contam-grpc/build
RUN cmake -DCMAKE_INSTALL_PREFIX=/app/contam-grpc/install .. && make -j 4 && fakeroot make install
WORKDIR /app/contam-grpc/install

# Set default command to bash
ENTRYPOINT ["/bin/bash"]
