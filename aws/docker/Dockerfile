FROM gnssanalysis/ginan-env:latest

ENV HOME /root
RUN mkdir -p /ginan
ADD . /ginan
WORKDIR /ginan
RUN mkdir -p src/build
WORKDIR /ginan/src/build
RUN cmake -DENABLE_OPTIMISATION=TRUE ..
RUN make

ENV PATH "/ginan/bin:${PATH}"
