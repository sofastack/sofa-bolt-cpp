FROM gcc:4

RUN echo "deb http://ftp.debian.org/debian jessie-backports main" > /etc/apt/sources.list.d/jessie-backports.list \
  && apt-get update && apt-get -t jessie-backports install -y --no-install-recommends \
         cmake \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/* /etc/apt/sources.list.d/jessie-backports.list
RUN wget https://cmake.org/files/v3.12/cmake-3.12.1.tar.gz
RUN tar xvf cmake-3.12.1.tar.gz
RUN ls
RUN cd cmake-3.12.1
WORKDIR cmake-3.12.1
RUN gcc --version
RUN ./configure
RUN make
RUN make install
RUN /usr/local/bin/cmake --version

RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v3.6.1/protobuf-cpp-3.6.1.tar.gz
RUN tar xvf protobuf-cpp-3.6.1.tar.gz
RUN ls
WORKDIR protobuf-3.6.1
RUN ./configure
RUN make
RUN make install
RUN ldconfig


RUN wget https://github.com/google/googletest/archive/release-1.8.0.tar.gz
RUN tar xvf release-1.8.0.tar.gz
RUN cd googletest-release-1.8.0
WORKDIR googletest-release-1.8.0
RUN cmake .
RUN make
RUN make install


ENV TIME_ZONE=Asia/Shanghai

RUN \
  mkdir -p /usr/src/app \
  && echo "${TIME_ZONE}" > /etc/timezone \
  && ln -sf /usr/share/zoneinfo/${TIME_ZONE} /etc/localtime

RUN protoc --version
RUN gcc --version
RUN cmake --version
COPY . /usr/src/myapp
WORKDIR /usr/src/myapp
RUN ls
RUN cmake .
RUN make
RUN ls
CMD ["./cloud_rpc_demo"]