FROM ubuntu:focal

ENV DEBIAN_FRONTEND=noninteractive

WORKDIR /app/build
RUN mkdir -p ./app/build

RUN apt-get update && \
    apt-get install -yy curl gpg

RUN curl https://installer.id.ee/media/install-scripts/C6C83D68.pub | gpg --dearmor | tee /etc/apt/trusted.gpg.d/ria-repository.gpg > /dev/null && \
    echo "deb http://installer.id.ee/media/ubuntu/ focal main" > /etc/apt/sources.list.d/ria-repository.list

RUN apt-get update && \
    apt-get install -yy build-essential qttools5-dev libqt5svg5-dev qttools5-dev-tools libpcsclite-dev libssl-dev libldap2-dev gettext pkg-config \
    cmake \
    libdigidocpp-dev