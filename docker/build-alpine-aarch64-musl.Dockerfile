FROM alpine:edge

ARG LLVM_VERSION

ENV CLANG_INSTALL_LINUX=/usr/local/llvm/install-linux

RUN set -eux; \
    test -n "$LLVM_VERSION"; \
    llvm_major="${LLVM_VERSION%%.*}"; \
    apk --no-cache add \
        build-base \
        "clang$llvm_major-dev~$LLVM_VERSION" \
        "clang$llvm_major-static~$LLVM_VERSION" \
        cmake \
        file \
        g++ \
        git \
        gtest \
        gtest-dev \
        "libc++-dev~$LLVM_VERSION" \
        libxml2-static \
        "llvm$llvm_major-dev~$LLVM_VERSION" \
        "llvm$llvm_major-gtest~$LLVM_VERSION" \
        "llvm$llvm_major-static~$LLVM_VERSION" \
        neovim \
        ninja-build \
        ninja-is-really-ninja \
        python3 \
        tar \
        zlib-static \
        zstd-static; \
    mkdir -p "$(dirname "$CLANG_INSTALL_LINUX")"; \
    ln -s "/usr/lib/llvm$llvm_major" "$CLANG_INSTALL_LINUX"

# Keep bundled libc++ headers usable on both musl and glibc targets.
COPY docker/libcxx-installed-config-site.patch /tmp/

RUN set -eux; \
    llvm_major="${LLVM_VERSION%%.*}"; \
    patch --no-backup-if-mismatch \
        -d / -p0 -i /tmp/libcxx-installed-config-site.patch; \
    resource_include="$CLANG_INSTALL_LINUX/lib/clang/$llvm_major/include"; \
    mkdir -p "$resource_include/c++/v1"; \
    cp -r /usr/include/c++/v1/* "$resource_include/c++/v1"; \
    rm /tmp/libcxx-installed-config-site.patch

# Alpine ships static Clang archives, but their CMake exports depend on the
# shared monolithic LLVM target. Redirect that target to LLVM's authoritative
# static component set so downstream fully-static links remain self-contained.
RUN set -eux; \
    llvm_major="${LLVM_VERSION%%.*}"; \
    llvm_config="llvm-config-$llvm_major"; \
    llvm_lib_dir="$CLANG_INSTALL_LINUX/lib"; \
    llvm_archive="$llvm_lib_dir/libLLVM-omnirefl.a"; \
    llvm_exports="$llvm_lib_dir/cmake/llvm/LLVMExports-release.cmake"; \
    llvm_shared="$(basename "$(readlink "$llvm_lib_dir/libLLVM.so")")"; \
    { \
        echo 'GROUP ('; \
        "$llvm_config" --link-static --libs \
            windowsdriver plugins option frontendopenmp scalaropts \
            aggressiveinstcombine instcombine frontendoffloading \
            transformutils objectyaml frontendatomic analysis frontendhlsl \
            profiledata symbolize debuginfogsym debuginfopdb \
            debuginfocodeview debuginfomsf debuginfobtf debuginfodwarf object \
            irreader bitreader asmparser core remarks bitstreamreader \
            mcparser mc textapi debuginfodwarflowlevel binaryformat \
            frontenddirective targetparser support demangle \
            | tr ' ' '\n' \
            | sed -E "s@^-l(.*)@$llvm_lib_dir/lib\\1.a@"; \
        "$llvm_config" --link-static --system-libs | tr ' ' '\n'; \
        echo ')'; \
    } > "$llvm_archive"; \
    sed -i "s@$llvm_shared@$(basename "$llvm_archive")@g" "$llvm_exports"

CMD ["ash"]
