# stage 1 build-clang
FROM alpine:3.23.3 AS builder

ARG ARCH=x86_64
ARG LLVM_VERSION

ENV DEBIAN_FRONTED=noninteractive

ENV LLVM_DIR=/tmp/llvm
ENV CLANG_BUILD_LINUX=/tmp/llvm/build-linux
ENV CLANG_BUILD_WINDOWS=/tmp/llvm/build-windows
ENV CLANG_BUILD_HEADERS=/tmp/llvm/build-headers

ENV CLANG_INSTALL_LINUX=/llvm/install-linux
ENV CLANG_INSTALL_WINDOWS=/llvm/install-windows
ENV CLANG_INSTALL_HEADERS=/llvm/install-headers

ENV CLANG_BUILD_TYPE=Release
ENV AARCH64_ALPINE_SYSROOT=/opt/aarch64-alpine-linux-musl
ENV LLVM_MINGW_ROOT=/opt/llvm-mingw
ENV LLVM_MINGW_VERSION=20260616

RUN case "$ARCH" in x86_64 | aarch64) ;; *) echo "unsupported ARCH=$ARCH" >&2; exit 1 ;; esac

RUN apk update && apk upgrade && \
    apk --no-cache add \
        build-base \
        clang21 \
        cmake \
        curl \
        g++ \
        git \
        gtest \
        gtest-dev \
        lld21 \
        mingw-w64-gcc \
        neovim \
        ninja-build \
        ninja-is-really-ninja \
        python3 \
        tar \
        xz \
        &&:

RUN mkdir $LLVM_DIR; cd $LLVM_DIR; \
    test -n "$LLVM_VERSION"; \
    git clone --depth 1 --branch llvmorg-$LLVM_VERSION https://github.com/llvm/llvm-project.git \
    &&:

RUN if [ aarch64 = "$ARCH" ]; then \
        mkdir -p "$AARCH64_ALPINE_SYSROOT/etc/apk"; \
        printf "%s\n" \
          https://dl-cdn.alpinelinux.org/alpine/v3.23/main \
          https://dl-cdn.alpinelinux.org/alpine/v3.23/community \
          > "$AARCH64_ALPINE_SYSROOT/etc/apk/repositories"; \
        # ad hoc: `apk --root --arch aarch64` does not trust the alternate
        # root repositories in this minimal setup even with the host keys
        # copied/passed explicitly. The repositories are the same pinned Alpine
        # version used by the image, so keep the exception local to this
        # throwaway cross sysroot.
        apk --root "$AARCH64_ALPINE_SYSROOT" \
          --arch aarch64 \
          --initdb \
          --allow-untrusted \
          add --no-cache musl-dev g++ linux-headers zlib-dev; \
        curl -fL \
          "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VERSION/llvm-mingw-$LLVM_MINGW_VERSION-msvcrt-ubuntu-22.04-x86_64.tar.xz" \
          -o /tmp/llvm-mingw.tar.xz; \
        mkdir -p "$LLVM_MINGW_ROOT"; \
        tar -xJf /tmp/llvm-mingw.tar.xz \
          -C "$LLVM_MINGW_ROOT" \
          --strip-components=1; \
    else \
        mkdir -p "$AARCH64_ALPINE_SYSROOT" "$LLVM_MINGW_ROOT"; \
    fi

# should it be passed as an argument?
ADD ./clang-cmake-options $LLVM_DIR/
RUN mkdir $CLANG_BUILD_LINUX; \
    cd $CLANG_BUILD_LINUX; \
    if [ x86_64 = "$ARCH" ]; then \
        CXX=g++ LDFLAGS=-static \
            cmake ../llvm-project/llvm -GNinja \
            -DCMAKE_BUILD_TYPE=$CLANG_BUILD_TYPE \
            -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_LINUX \
            -DLLVM_ENABLE_PROJECTS="clang" \
            -DLLVM_TARGETS_TO_BUILD=X86 \
            $(cat $LLVM_DIR/clang-cmake-options); \
    else \
        CC=clang-21 CXX=clang++-21 \
            cmake ../llvm-project/llvm -GNinja \
            -DCMAKE_BUILD_TYPE=$CLANG_BUILD_TYPE \
            -DCMAKE_SYSTEM_NAME=Linux \
            -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
            -DCMAKE_C_COMPILER_TARGET=aarch64-alpine-linux-musl \
            -DCMAKE_CXX_COMPILER_TARGET=aarch64-alpine-linux-musl \
            -DCMAKE_SYSROOT="$AARCH64_ALPINE_SYSROOT" \
            -DCMAKE_CXX_FLAGS="-stdlib=libstdc++" \
            -DCMAKE_EXE_LINKER_FLAGS="-static -fuse-ld=lld" \
            -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_LINUX \
            -DLLVM_DEFAULT_TARGET_TRIPLE=aarch64-alpine-linux-musl \
            -DLLVM_ENABLE_PROJECTS="clang" \
            -DLLVM_TARGETS_TO_BUILD=AArch64 \
            $(cat $LLVM_DIR/clang-cmake-options); \
    fi
RUN cmake --build $CLANG_BUILD_LINUX -j$(nproc)
RUN cmake --install $CLANG_BUILD_LINUX
    
RUN mkdir $CLANG_BUILD_WINDOWS; \
    cd $CLANG_BUILD_WINDOWS; \
    if [ x86_64 = "$ARCH" ]; then \
        CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ LDFLAGS=-static \
            cmake ../llvm-project/llvm -GNinja \
            -DCMAKE_BUILD_TYPE=$CLANG_BUILD_TYPE \
            -DCMAKE_SYSTEM_NAME=Windows \
            -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_WINDOWS \
            -DLLVM_ENABLE_PROJECTS="clang" \
            -DLLVM_TARGETS_TO_BUILD=X86 \
            $(cat $LLVM_DIR/clang-cmake-options); \
    else \
        MINGW_RESOURCE_DIR="$(find "$LLVM_MINGW_ROOT/lib/clang" \
            -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1)"; \
        CC=clang-21 CXX=clang++-21 \
            cmake ../llvm-project/llvm -GNinja \
            -DCMAKE_BUILD_TYPE=$CLANG_BUILD_TYPE \
            -DCMAKE_SYSTEM_NAME=Windows \
            -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
            -DCMAKE_C_COMPILER_TARGET=aarch64-w64-windows-gnu \
            -DCMAKE_CXX_COMPILER_TARGET=aarch64-w64-windows-gnu \
            -DCMAKE_SYSROOT="$LLVM_MINGW_ROOT/aarch64-w64-mingw32" \
            -DCMAKE_C_FLAGS="-resource-dir $MINGW_RESOURCE_DIR -rtlib=compiler-rt" \
            -DCMAKE_CXX_FLAGS="-resource-dir $MINGW_RESOURCE_DIR -stdlib=libc++ -rtlib=compiler-rt" \
            -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind" \
            -DCMAKE_INSTALL_PREFIX=$CLANG_INSTALL_WINDOWS \
            -DLLVM_DEFAULT_TARGET_TRIPLE=aarch64-w64-windows-gnu \
            -DLLVM_ENABLE_PROJECTS="clang" \
            -DLLVM_TARGETS_TO_BUILD=AArch64 \
            $(cat $LLVM_DIR/clang-cmake-options); \
    fi
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

ARG ARCH=x86_64

ENV DEBIAN_FRONTED=noninteractive

ENV CLANG_INSTALL_LINUX=/usr/local/llvm/install-linux
ENV CLANG_INSTALL_WINDOWS=/usr/local/llvm/install-windows
ENV AARCH64_ALPINE_SYSROOT=/opt/aarch64-alpine-linux-musl
ENV LLVM_MINGW_ROOT=/opt/llvm-mingw
ENV PATH=/usr/lib/llvm21/bin:$PATH

RUN case "$ARCH" in x86_64 | aarch64) ;; *) echo "unsupported ARCH=$ARCH" >&2; exit 1 ;; esac

# how can I reuse the $CLANG_INSTALL_LINUX from builder?
COPY --from=builder /llvm/install-linux $CLANG_INSTALL_LINUX
COPY --from=builder /llvm/install-windows $CLANG_INSTALL_WINDOWS
COPY --from=builder /opt/aarch64-alpine-linux-musl $AARCH64_ALPINE_SYSROOT
COPY --from=builder /opt/llvm-mingw $LLVM_MINGW_ROOT

RUN apk update && apk upgrade && \
    apk --no-cache add \
        build-base \
        cmake \
        g++ \
        git \
        gtest \
        gtest-dev \
        neovim \
        ninja-build \
        ninja-is-really-ninja \
        python3 \
        tar \
        && if [ x86_64 = "$ARCH" ]; then \
            apk --no-cache add mingw-w64-gcc; \
        else \
            apk --no-cache add clang21 lld21 llvm21; \
        fi \
        && rm -rf /var/cache/apk/*

CMD ["ash"]
