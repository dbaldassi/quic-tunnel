FROM sharkalash/quic-tunnel-env

# ENV CC "/opt/gcc-12/bin/gcc"
# ENV CXX "/opt/gcc-12/bin/g++"
# ENV LD_LIBRARY_PATH "/opt/gcc-12/lib/../lib64"

WORKDIR /root

RUN mkdir quic-forwarding

WORKDIR quic-forwarding

COPY external external
COPY cmake cmake
COPY CMakeLists.txt CMakeLists.txt
COPY src src

RUN mkdir build

WORKDIR build

RUN cmake .. -DCMAKE_BUILD_TYPE=Release \
    -Dmvfst_DIR=/opt/mvfst/lib/cmake/mvfst \
    -Dfolly_DIR=/opt/mvfst/lib/cmake/folly \
    -DFizz_DIR=/opt/mvfst/lib/cmake/fizz   \
    -Dfmt_DIR=/opt/mvfst/lib/cmake/fmt

RUN make

RUN mkdir tunnel-in-logs
RUN mkdir tunnel-out-logs

ENTRYPOINT [ "./quic-tunnel" ]
