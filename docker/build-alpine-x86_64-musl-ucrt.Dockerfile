# stage 1 build-clang
FROM alpine:3.23.3 AS builder

ENV DEBIAN_FRONTED=noninteractive

ENV LLVM_DIR=/tmp/llvm
ENV CLANG_BUILD_LINUX=/tmp/llvm/build-linux
ENV CLANG_BUILD_WINDOWS=/tmp/llvm/build-windows
ENV CLANG_BUILD_HEADERS=/tmp/llvm/build-headers

ENV CLANG_INSTALL_LINUX=/llvm/install-linux
ENV CLANG_INSTALL_WINDOWS=/llvm/install-windows
ENV CLANG_INSTALL_HEADERS=/llvm/install-headers

ENV CLANG_BUILD_TYPE=Release

RUN apk update && apk upgrade && \
    apk --no-cache add \
        build-base \
        clang21 \
        cmake \
        g++ \
        git \
        gtest \
        gtest-dev \
        mingw-w64-gcc \
        neovim \
        ninja-build \
        ninja-is-really-ninja \
        python3 \
        tar \
        &&:

ARG LLVM_VERSION
RUN mkdir $LLVM_DIR; cd $LLVM_DIR; \
    test -n "$LLVM_VERSION"; \
    git clone --depth 1 --branch llvmorg-$LLVM_VERSION https://github.com/llvm/llvm-project.git \
    &&:

# should it be passed as an argument?
ADD ./clang-cmake-options $LLVM_DIR/
RUN mkdir $CLANG_BUILD_LINUX; \
    cd $CLANG_BUILD_LINUX; \
    CXX=g++ LDFLAGS=-static \
        cmake ../llvm-project/llvm -GNinja \
        -DCMAKE_BUILD_TYPE=$CLANG_BUILD_TYPE \
        -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_LINUX \
        -DLLVM_ENABLE_PROJECTS="clang" \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        $(cat $LLVM_DIR/clang-cmake-options) \
        &&:
RUN cmake --build $CLANG_BUILD_LINUX -j$(nproc)
RUN cmake --install $CLANG_BUILD_LINUX

RUN mkdir $CLANG_BUILD_WINDOWS; \
    cd $CLANG_BUILD_WINDOWS; \
    CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ LDFLAGS=-static \
        cmake ../llvm-project/llvm -GNinja \
        -DCMAKE_BUILD_TYPE=$CLANG_BUILD_TYPE \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_WINDOWS \
        -DLLVM_ENABLE_PROJECTS="clang" \
        -DLLVM_TARGETS_TO_BUILD=X86 \
        $(cat $LLVM_DIR/clang-cmake-options) \
    &&:
RUN cmake --build $CLANG_BUILD_WINDOWS -j$(nproc)
RUN cmake --install $CLANG_BUILD_WINDOWS

# should it be passed as an argument?
ADD ./llvm-project.patch $LLVM_DIR/
RUN cd $LLVM_DIR/llvm-project; \
    # ad hoc: can't build without LIBCXX_HAS_MUSL_LIBC on alpine, and it will be configured.
    #   but the tool will fail on ubuntu, because of missing "bits/alltypes.h"
    git apply ../llvm-project.patch

RUN mkdir -p "$CLANG_BUILD_HEADERS"; \
    cd "$CLANG_BUILD_HEADERS"; \
    CC=clang-21 CXX=clang++-21 \
        cmake ../llvm-project/runtimes -GNinja \
        -DCMAKE_INSTALL_PREFIX="$CLANG_INSTALL_HEADERS" \
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
        -DLLVM_INCLUDE_TESTS=OFF \
        -DLIBCXX_INCLUDE_TESTS=OFF \
        -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
        -DLIBCXX_INSTALL_LIBRARY=OFF \
        -DLIBCXXABI_INCLUDE_TESTS=OFF \
        -DLIBCXXABI_INSTALL_LIBRARY=OFF \
        -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
    && cmake --build . --target install-cxx-headers install-cxxabi-headers -- -j"$(nproc)" \
    && cp -r "$CLANG_INSTALL_HEADERS/include/"* "$CLANG_INSTALL_LINUX/lib/clang/"*/include \
    && cp -r "$CLANG_INSTALL_HEADERS/include/"* "$CLANG_INSTALL_WINDOWS/lib/clang/"*/include \
    && :

# stage 2 final image
FROM alpine:3.23.3

ENV DEBIAN_FRONTED=noninteractive

ENV CLANG_INSTALL_LINUX=/usr/local/llvm/install-linux
ENV CLANG_INSTALL_WINDOWS=/usr/local/llvm/install-windows

# how can I reuse the $CLANG_INSTALL_LINUX from builder?
COPY --from=builder /llvm/install-linux $CLANG_INSTALL_LINUX
COPY --from=builder /llvm/install-windows $CLANG_INSTALL_WINDOWS

RUN apk update && apk upgrade && \
    apk --no-cache add \
        build-base \
        cmake \
        g++ \
        git \
        gtest \
        gtest-dev \
        mingw-w64-gcc \
        neovim \
        ninja-build \
        ninja-is-really-ninja \
        python3 \
        tar \
        && rm -rf /var/cache/apk/*

CMD ["ash"]
