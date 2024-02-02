FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential           \
    g++                       \
    g++-12                    \
    gcc-12                    \
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
    libunwind-dev             \
    libzstd-dev               \
    libdwarf-dev              \
    libgmock-dev              \
    libbz2-dev                \
    libaio-dev                \
    make                      \
    zlib1g-dev                \
    binutils-dev              \
    libjemalloc-dev           \
    libssl-dev                \
    pkg-config                \
    libsodium-dev             \
    iptables                  \
    tcpdump                   \
    python3                   \
    python3-pip               \
    socat                     \
    iproute2                  \
    moreutils                 \
    libfmt-dev                \
    libev-dev                 \
    gdb

RUN pip3 install matplotlib

WORKDIR /opt

COPY mvfst-fpic mvfst
COPY go go
COPY lsquic_so_fpic lsquic
COPY quiche quiche
COPY boringssl boringssl
COPY cert.pem cert.pem
COPY key.pem key.pem
COPY ev ev
COPY liburing liburing

ENV PATH="${PATH}:/opt/go/bin"

# COPY gcc-12 gcc-12

WORKDIR /usr/lib/x86_64-linux-gnu/

RUN ln -s librt.so.1 librt.so

WORKDIR /usr/bin

RUN rm gcc g++
RUN ln -s gcc-12 gcc
RUN ln -s g++-12 g++


WORKDIR /usr/local/lib

RUN ln -s /usr/lib/libglog.so

WORKDIR /usr/local/bin

ADD tcpdumpbitrate.py .
ADD show_csv.py .
ADD show_csv_ss.py .
ADD ss-output.sh .

RUN chmod +x tcpdumpbitrate.py
RUN chmod +x show_csv.py
RUN chmod +x show_csv_ss.py
RUN chmod +x ss-output.sh
