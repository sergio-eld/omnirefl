# stage 1 build-clang
FROM alpine:3.20.3 AS builder

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
        clang18 \
        cmake \
        g++ \
        git \
        gtest \
        gtest-dev \
        mingw-w64-gcc \
        neovim \
        ninja \
        python3 \
        tar \
        &&:

RUN mkdir $LLVM_DIR; cd $LLVM_DIR; \
    git clone --depth 1 --branch release/19.x https://github.com/llvm/llvm-project.git \
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
    
# should it be passed as an argument?
ADD ./llvm-project.patch $LLVM_DIR/
RUN cd $LLVM_DIR/llvm-project; \
    # ad hoc: can't build without LIBCXX_HAS_MUSL_LIBC on alpine, and it will be configured.
    #   but the tool will fail on ubuntu, because of missing "bits/alltypes.h"
    git apply ../llvm-project.patch

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

RUN mkdir $CLANG_BUILD_HEADERS; \
    cd $CLANG_BUILD_HEADERS; \
    CC=clang-18 CXX=clang++-18 \
        cmake ../llvm-project/runtimes -GNinja \
        # -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_HEADERS \
        -DLLVM_INCLUDE_TESTS=OFF \
        # -DLLVM_TARGETS_TO_BUILD=X86 \
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
        -DBENCHMARK_ENABLE_EXCEPTIONS=OFF \
        -DBENCHMARK_INSTALL_DOCS=OFF \
        -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
        -DCOMPILER_RT_BUILD_MEMPROF=OFF \
        -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
        -DCOMPILER_RT_BUILD_XRAY=OFF \
        -DLIBCXXABI_ENABLE_ASSERTIONS=OFF \
        # -DLIBCXXABI_ENABLE_EXCEPTIONS=OFF \
        -DLIBCXXABI_ENABLE_NEW_DELETE_DEFINITIONS=OFF \
        -DLIBCXXABI_ENABLE_SHARED=OFF \
        -DLIBCXXABI_ENABLE_THREADS=OFF \
        -DLIBCXXABI_INCLUDE_TESTS=OFF \
        -DLIBCXXABI_INSTALL_LIBRARY=OFF \
        -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
        -DLIBCXX_ENABLE_ABI_LINKER_SCRIPT=OFF \
        # -DLIBCXX_ENABLE_FILESYSTEM=OFF \
        -DLIBCXX_ENABLE_SHARED=OFF \
        -DLIBCXX_ENABLE_STATIC=OFF \
        # -DLIBCXX_HAS_MUSL_LIBC=ON \ # handled by llvm-project.patch
        -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
        -DLIBCXX_INCLUDE_TESTS=OFF \
        -DLIBCXX_INSTALL_LIBRARY=OFF \
        -DLLVM_BUILD_RUNTIME=OFF \
    && cmake --build . -j$(nproc) \
    && cmake --install . \
    && cp -r $CLANG_INSTALL_HEADERS/include/* $CLANG_INSTALL_LINUX/lib/clang/*/include \
    && cp -r $CLANG_INSTALL_HEADERS/include/* $CLANG_INSTALL_WINDOWS/lib/clang/*/include \
    &&:

# stage 2 final image
FROM alpine:3.20.3

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
        ninja \
        python3 \
        tar \
        && rm -rf /var/cache/apk/*

CMD ["ash"]
