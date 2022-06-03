FROM debian:bullseye

RUN apt-get update && apt-get install -y \
    build-essential           \
    g++                       \
    cmake                     \
    m4                        \
    libboost-all-dev          \
    libevent-dev              \
    libdouble-conversion-dev  \
    libgoogle-glog-dev        \
    libgflags-dev             \
    libiberty-dev             \
    liblz4-dev                \
    liblzma-dev               \
    libsnappy-dev             \
    make                      \
    zlib1g-dev                \
    binutils-dev              \
    libjemalloc-dev           \
    libssl-dev                \
    pkg-config                \
    libsodium-dev

WORKDIR /root

RUN mkdir quic-forwarding

WORKDIR quic-forwarding

COPY . .

RUN mkdir build

WORKDIR build

ENV CC "/root/quic-forwarding/gcc-12/bin/gcc"
ENV CXX "/root/quic-forwarding/gcc-12/bin/g++"
ENV LD_LIBRARY_PATH "/root/quic-forwarding/gcc-12/lib/../lib64"

RUN cmake .. \
    -Dmvfst_DIR=mvfst/lib/cmake/mvfst \
    -Dfolly_DIR=mvfst/lib/cmake/folly \
    -DFizz_DIR=mvfst/lib/cmake/fizz   \
    -Dfmt_DIR=mvfst/lib/cmake/fmt

RUN make

RUN mkdir tunnel-in-logs

ENTRYPOINT [ "./quic-tunnel" ]
