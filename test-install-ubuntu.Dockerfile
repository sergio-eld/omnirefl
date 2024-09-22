ARG UBUNTU_VERSION=18.04
FROM ubuntu:$UBUNTU_VERSION

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update \
    && apt install -y \
        build-essential \
        clang \
        g++ \
        git \
        libgtest-dev \
        neovim \
        ninja-build \
        wget \
    && apt clean -y

ADD https://github.com/Kitware/CMake/releases/download/v3.30.3/cmake-3.30.3.tar.gz /tmp/cmake/
RUN cd /tmp/cmake \
    ; tar --strip-components=1 -zxf ./cmake-*.tar.gz \
    && ./configure --generator=Ninja \
        --parallel=$(nproc) \
        --no-qt-gui \
        --no-debugger \
        -- -DCMAKE_USE_OPENSSL=OFF \
    && ninja -j$(nproc) \
    && ninja install \
    && cd / \
    && rm -rf /tmp/cmake

RUN mkdir /tmp/gtest \
    ; cd /tmp/gtest \
    ; cmake /usr/src/gtest -GNinja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . --target install -j$(nproc) \
    && cd /; rm -rf /tmp/gtest \
    && apt remove libgtest-dev -y

RUN rm -rf /var/lib/apt/lists/*
CMD ["bash"]
