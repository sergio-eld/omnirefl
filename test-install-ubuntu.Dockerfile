# Usage example:
#   docker build -f test-install-ubuntu.Dockerfile \
#     --build-arg UBUNTU_VERSION=18.04 \
#     --build-arg COMPILER=gcc   \  # or clang / mingw
#     -t sergioeld/test-install-ubuntu-18-gcc .

ARG UBUNTU_VERSION=18.04
ARG COMPILER=gcc   # gcc | clang | mingw

FROM ubuntu:${UBUNTU_VERSION} AS build

ARG COMPILER
ENV DEBIAN_FRONTEND=noninteractive

RUN apt update \
    && apt install -y \
        build-essential \
        g++ \
        git \
        ninja-build \
        unzip \
        curl \
        ca-certificates \
    && if [ "$COMPILER" = "clang" ]; then \
           apt install -y clang; \
       elif [ "$COMPILER" = "mingw" ]; then \
           apt install -y mingw-w64; \
       fi \
    && apt clean -y \
    && rm -rf /var/lib/apt/lists/*

ARG CMAKE_VERSION=3.27.6
ADD https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}.tar.gz /tmp/cmake.tar.gz

RUN set -eux; \
    mkdir -p /tmp/cmake; \
    tar -zxf /tmp/cmake.tar.gz -C /tmp/cmake --strip-components=1; \
    cd /tmp/cmake; \
    ./configure \
        --generator=Ninja \
        --parallel="$(nproc)" \
        --no-qt-gui \
        --no-debugger \
        -- -DCMAKE_USE_OPENSSL=OFF; \
    ninja -j"$(nproc)"; \
    mkdir -p /opt/pkg/cmake; \
    DESTDIR=/opt/pkg/cmake ninja install; \
    mkdir -p /opt/installers; \
    tar -czf /opt/installers/cmake.tar.gz -C /opt/pkg/cmake .; \
    rm -rf /opt/pkg/cmake

ARG GTEST_VERSION=1.14.0
ADD https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz /tmp/gtest.tar.gz

RUN set -eux; \
    if [ "$COMPILER" = "gcc" ]; then \
        CC=gcc; CXX=g++; \
    elif [ "$COMPILER" = "clang" ]; then \
        CC=clang; CXX=clang++; \
    elif [ "$COMPILER" = "mingw" ]; then \
        CC=x86_64-w64-mingw32-gcc; \
        CXX=x86_64-w64-mingw32-g++; \
    else \
        echo "Unsupported COMPILER '$COMPILER' (expected gcc|clang|mingw)"; \
        exit 1; \
    fi; \
    mkdir -p /tmp/gtest; \
    tar -zxf /tmp/gtest.tar.gz -C /tmp/gtest --strip-components=1; \
    /tmp/cmake/bin/cmake -S /tmp/gtest -B /tmp/gtest/build \
        -G Ninja \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DBUILD_GTEST=ON \
        -DBUILD_GMOCK=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_INSTALL_PREFIX=/usr/local; \
    /tmp/cmake/bin/cmake --build /tmp/gtest/build --parallel; \
    mkdir -p /opt/pkg/gtest; \
    DESTDIR=/opt/pkg/gtest /tmp/cmake/bin/cmake --install /tmp/gtest/build; \
    mkdir -p /opt/installers; \
    tar -czf /opt/installers/gtest.tar.gz -C /opt/pkg/gtest .; \
    rm -rf /opt/pkg/gtest /tmp/gtest /tmp/gtest.tar.gz /tmp/cmake /tmp/cmake.tar.gz

FROM ubuntu:${UBUNTU_VERSION} AS final

ARG COMPILER
ENV DEBIAN_FRONTEND=noninteractive

RUN set -eux; \
    apt update; \
    apt install -y \
        git \
        neovim \
        ninja-build; \
    if [ "$COMPILER" = "gcc" ] || [ "$COMPILER" = "clang" ]; then \
        apt install -y build-essential g++; \
    fi; \
    if [ "$COMPILER" = "clang" ]; then \
        apt install -y clang; \
    elif [ "$COMPILER" = "mingw" ]; then \
        apt install -y mingw-w64; \
    fi; \
    apt clean -y; \
    rm -rf /var/lib/apt/lists/*

# Bring in installers from build stage
COPY --from=build /opt/installers /opt/installers

RUN set -eux; \
    tar -xzf /opt/installers/cmake.tar.gz -C /; \
    tar -xzf /opt/installers/gtest.tar.gz -C /; \
    rm -rf /opt/installers

CMD ["bash"]
