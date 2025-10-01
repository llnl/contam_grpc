FROM fedora:latest

# Install necessary dependencies
RUN dnf -y install --skip-unavailable epel-release
RUN dnf -y install fakeroot
RUN fakeroot dnf upgrade -y && fakeroot dnf update -y
RUN fakeroot dnf install -y wget hostname libevent
RUN fakeroot dnf install -y awk gawk
RUN fakeroot dnf install -y vim patch make cmake gzip git gcc-c++ python3 python3-pip python3-devel
RUN fakeroot dnf install -y glibc-locale-source
RUN fakeroot dnf install -y gdb clang-format
RUN fakeroot python3 -m pip install --upgrade pip
RUN fakeroot pip3 install --no-cache-dir cffi numpy pycparser
RUN fakeroot pip3 install contamxpy
RUN localedef --inputfile=en_US --charmap=UTF-8 en_US.UTF-8
ENV LANG=en_US.UTF-8

# Download and build gRPC
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

# Download and build ExaEpi
RUN fakeroot git clone https://github.com/AMReX-Agent/ExaEpi.git

# Set working directory
WORKDIR /app
RUN fakeroot wget http://www.nist.gov/system/files/documents/noindex/2021/11/09/contam-x-3.4.0.1-Linux-64bit.tar.gz
#RUN fakeroot wget https://files.pythonhosted.org/packages/64/14/bdc628c19fdd0222b2cb2d06b075fe07dbee1770e30dd3cbc720571e05aa/contamxpy-0.0.9.tar.gz
COPY *.tar.gz /app

# Copy and extract any *.tgz files
RUN for archive in *.tar.gz; do \
    if [ -f "$archive" ]; then \
        echo "Extracting $archive..."; \
        tar -xzf "$archive" --no-same-owner -C /app; \
    fi; \
    done

WORKDIR /app/runContamxDll_OneZoneWthCtm/contamx-artifacts/3.4.1.10
RUN tar zxvf contam-x-3.4.1.10-Linux-64bit-cxlibShared-debian-bookworm.tar.gz --no-same-owner
ENV CONTAM_LIB_DIR=/app/runContamxDll_OneZoneWthCtm/contamx-artifacts/3.4.1.10/contam-x-3.4.1.10-Linux-64bit-cxlibShared
ENV CONTAM_INC_DIR=${CONTAM_LIB_DIR}/include/contam-x-inc

WORKDIR /app/contam_grpc/build
RUN cmake -DCMAKE_INSTALL_PREFIX=/app/contam_grpc/install .. && make -j 4 && fakeroot make install
WORKDIR /app/contam_grpc/install/bin
RUN echo 'setl noai nocin nosi inde= paste ruler number' > ~/.vimrc

# Set default command to bash
ENTRYPOINT ["/bin/bash"]
