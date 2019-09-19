FROM ubuntu:latest
LABEL maintainer='Nathaniel van Diepen'

WORKDIR /root

RUN apt-get update
RUN apt-get dist-upgrade -y
RUN apt-get install -y \
  curl \
  qtcreator \
  build-essential \
  gcc-arm-linux-gnueabihf

RUN curl https://remarkable.engineering/deploy/sdk/poky-glibc-x86_64-meta-toolchain-qt5-cortexa9hf-neon-toolchain-2.1.3.sh \
  -o install-toolchain.sh
RUN chmod +x install-toolchain.sh
RUN ./install-toolchain.sh -y \
  -d /opt/poky/2.1.3/

RUN curl https://sh.rustup.rs -sSf | sh -s -- --default-toolchain nightly -y
ENV PATH="${PATH}:/root/.cargo/bin"
RUN rustup target add armv7-unknown-linux-gnueabihf
RUN mkdir -p "/root/.cargo/registry"
COPY files/.cargo/config /root/.cargo/config
RUN rustup update

RUN rm -rf .config
COPY files/config .config
COPY files/libqsgepaper.a .
COPY files/start-qt.sh .
RUN chmod +x ./start-qt.sh

WORKDIR /root/project

CMD ["/root/start-qt.sh"]
