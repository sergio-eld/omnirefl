ARG UBUNTU_VERSION=18.04
FROM ubuntu:$UBUNTU_VERSION AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update \
    && apt install -y \
        build-essential \
        g++ \
        git \
        ninja-build \
        unzip \
    && apt clean -y

# Build and install CMake
ARG CMAKE_VERSION=3.27.6
ADD https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION.tar.gz /tmp/cmake.tar.gz
RUN mkdir -p /tmp/cmake \
    && tar -zxf /tmp/cmake.tar.gz -C /tmp/cmake --strip-components=1 \
    && cd /tmp/cmake \
    && ./configure --generator=Ninja \
        --parallel=$(nproc) \
        --no-qt-gui \
        --no-debugger \
        -- -DCMAKE_USE_OPENSSL=OFF \
    && ninja -j$(nproc) \
    && ninja install \
    && rm -rf /tmp/cmake /tmp/cmake.tar.gz

# Build and install GoogleTest
ARG GTEST_VERSION=1.14.0
ADD https://github.com/google/googletest/archive/refs/tags/v$GTEST_VERSION.tar.gz /tmp/gtest.tar.gz
RUN mkdir -p /tmp/gtest \
    && tar -zxf /tmp/gtest.tar.gz -C /tmp/gtest --strip-components=1 \
    && cd /tmp/gtest \
    && cmake -GNinja -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_GMOCK=OFF \
        -Dgtest_build_tests=OFF \
        -Dgmock_build_tests=OFF \
    && cmake --build . --target install -j$(nproc) \
    && rm -rf /tmp/gtest /tmp/gtest.tar.gz

# todo: build latest neovim from source?

FROM ubuntu:$UBUNTU_VERSION AS final

ENV DEBIAN_FRONTEND=noninteractive

COPY --from=build /usr/local/ /usr/

COPY --from=build /usr/lib/libgtest* /usr/lib/

RUN apt update \
    && apt install -y \
        build-essential \
        g++ \
        neovim \
        ninja-build \
    && apt clean -y \
    && rm -rf /var/lib/apt/lists/*

CMD ["bash"]

