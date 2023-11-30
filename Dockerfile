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

RUN cmake .. -DCMAKE_BUILD_TYPE=Release \
    -Dmvfst_DIR=/opt/mvfst/lib/cmake/mvfst \
    -Dfolly_DIR=/opt/mvfst/lib/cmake/folly \
    -DFizz_DIR=/opt/mvfst/lib/cmake/fizz   \
    -Dfmt_DIR=/opt/mvfst/lib/cmake/fmt \
    -Dlsquic_DIR=/opt/lsquic/share/lsquic \
    -DQUICHE_DIR=/opt/quiche

RUN make

RUN mkdir tunnel-in-logs
RUN mkdir tunnel-out-logs

RUN cp /opt/cert.pem .
RUN cp /opt/key.pem .

ENTRYPOINT [ "./quic-tunnel" ]
