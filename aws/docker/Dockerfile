FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive
ENV HOME /root

RUN apt update -y
RUN apt install -y git gobjc gobjc++ gfortran libopenblas-dev openssl curl net-tools openssh-server cmake make liblapack-dev gzip vim libssl-dev awscli gnupg2 libnetcdf-dev libnetcdf-c++4-dev libzstd-dev libssl-dev doxygen libgomp1 graphviz

RUN mkdir -p /tmp/build

WORKDIR /tmp/build
run git clone https://github.com/jbeder/yaml-cpp.git
run mkdir yaml-cpp/cmake-build
WORKDIR /tmp/build/yaml-cpp/cmake-build
run cmake .. -DCMAKE\_INSTALL\_PREFIX=/usr/local/ -DYAML\_CPP\_BUILD\_TESTS=OFF
run make install yaml-cpp

WORKDIR /tmp/build
RUN git clone https://gitlab.com/libeigen/eigen.git
RUN mkdir eigen/cmake-build
WORKDIR /tmp/build/eigen/cmake-build
RUN cmake ..
RUN make install

WORKDIR /tmp/build
RUN wget https://github.com/mongodb/mongo-c-driver/releases/download/1.17.1/mongo-c-driver-1.17.1.tar.gz
RUN tar -xvf mongo-c-driver-1.17.1.tar.gz
RUN mkdir /tmp/build/mongo-c-driver-1.17.1/cmakebuild
WORKDIR /tmp/build/mongo-c-driver-1.17.1/cmakebuild
RUN cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
RUN cmake --build .
RUN cmake --build . --target install

WORKDIR /tmp/build
RUN curl -OL https://github.com/mongodb/mongo-cxx-driver/releases/download/r3.6.0/mongo-cxx-driver-r3.6.0.tar.gz
RUN tar -xzf mongo-cxx-driver-r3.6.0.tar.gz
RUN mkdir -p /tmp/build/mongo-cxx-driver-r3.6.0/build
WORKDIR /tmp/build/mongo-cxx-driver-r3.6.0/build
RUN cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
RUN cmake --build . --target EP_mnmlstc_core
RUN cmake --build .
RUN cmake --build . --target install

WORKDIR /tmp/build
RUN wget -qO - https://www.mongodb.org/static/pgp/server-4.4.asc | apt-key add -
RUN echo "deb [ arch=amd64,arm64 ] https://repo.mongodb.org/apt/ubuntu focal/mongodb-org/4.4 multiverse" | tee /etc/apt/sources.list.d/mongodb-org-4.4.list
RUN echo "deb [ arch=amd64,arm64 ] https://repo.mongodb.org/apt/ubuntu bionic/mongodb-org/4.4 multiverse" | tee /etc/apt/sources.list.d/mongodb-org-4.4.list
RUN apt update -y
RUN apt -y install mongodb-org

RUN echo installing conda ...
RUN wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O /tmp/build/miniconda.sh
RUN bash /tmp/build/miniconda.sh -b -p "$HOME/.miniconda3"
RUN rm /tmp/build/miniconda.sh
RUN echo installing conda completed.

COPY scripts/conda_gn37.yaml /tmp/build/conda_gn37.yaml
ENV PATH "${HOME}/.miniconda3/bin:${PATH}"
RUN conda env create -f /tmp/build/conda_gn37.yaml
COPY aws/docker/.bashrc "$HOME/.bashrc"

WORKDIR /tmp/build
RUN wget -c https://boostorg.jfrog.io/artifactory/main/release/1.75.0/source/boost_1_75_0.tar.gz
RUN gunzip boost_1_75_0.tar.gz
RUN tar xvf boost_1_75_0.tar
WORKDIR /tmp/build/boost_1_75_0/
RUN ./bootstrap.sh
RUN ./b2 install

RUN rm -rf /tmp/build

RUN mkdir -p /ginan
ADD . /ginan
WORKDIR /ginan
RUN mkdir -p src/build
WORKDIR /ginan/src/build
RUN cmake -DENABLE_OPTIMISATION=TRUE ..
RUN make

ENV PATH "/ginan/bin:${PATH}"
