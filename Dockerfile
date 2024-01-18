FROM sharkalash/quic-tunnel-env

WORKDIR /root

RUN mkdir quic-forwarding

WORKDIR quic-forwarding

COPY external external
COPY cmake cmake
COPY CMakeLists.txt CMakeLists.txt
COPY src src

RUN mkdir build

WORKDIR build

ENV LD_LIBRARY_PATH=/opt/mvfst/mvfst/lib:/opt/mvfst/fizz/lib:$PWD/opt/mvfst/folly/lib:/opt/liburing/lib

RUN cmake .. -DCMAKE_BUILD_TYPE=Release \
    -Dmvfst_DIR=/opt/mvfst/mvfst/lib/cmake/mvfst \
    -Dfolly_DIR=/opt/mvfst/folly/lib/cmake/folly \
    -DFizz_DIR=/opt/mvfst/fizz/lib/cmake/fizz \
    -Dfmt_DIR=/opt/mvfst/fmt/lib/cmake/fmt \
    -Dlsquic_DIR=/opt/lsquic/share/lsquic \
    -DQUICHE_DIR=/opt/quiche \
    -Dglog_DIR=/opt/mvfst/glog/lib/cmake/glog

RUN make

RUN mkdir tunnel-in-logs
RUN mkdir tunnel-out-logs

RUN cp /opt/cert.pem .
RUN cp /opt/key.pem .

ENTRYPOINT [ "./quic-tunnel" ]
