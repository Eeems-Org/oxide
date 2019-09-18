FROM ubuntu:latest

WORKDIR /root

RUN apt-get update
RUN apt-get dist-upgrade -y
RUN apt-get install -y \
  curl \
  qtcreator

RUN curl https://remarkable.engineering/deploy/sdk/poky-glibc-x86_64-meta-toolchain-qt5-cortexa9hf-neon-toolchain-2.1.3.sh \
  -o install-toolchain.sh
RUN chmod +x install-toolchain.sh
RUN ./install-toolchain.sh -y \
  -d /opt/poky/2.1.3/

RUN rm -rf .config
COPY files/config .config

COPY files/start-qt.sh .
RUN chmod +x ./start-qt.sh
COPY files/libqsgepaper.a .
RUN cd /usr/lib/x86_64-linux-gnu/qt5/plugins/

WORKDIR /root/project

CMD ["/root/start-qt.sh"]
