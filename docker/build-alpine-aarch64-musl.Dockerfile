# LLVM 22 is not available in Alpine 3.23. Pin the AArch64 edge image so
# unrelated edge updates do not invalidate every following build layer.
FROM alpine:edge@sha256:020dfcbaaf4cc1078bf2d9c7ba31a8466e334061dcd2f248001d68f79e52c000

ENV CLANG_INSTALL_LINUX=/usr/local/llvm/install-linux

RUN set -eux; \
    apk --no-cache add \
        build-base \
        cmake \
        git \
        libxml2-static \
        ninja-build \
        ninja-is-really-ninja \
        python3 \
        tar \
        zlib-static \
        zstd-static

ARG LLVM_VERSION

RUN set -eux; \
    test -n "$LLVM_VERSION"; \
    llvm_major="${LLVM_VERSION%%.*}"; \
    apk --no-cache add \
        "clang$llvm_major-dev~$LLVM_VERSION" \
        "clang$llvm_major-static~$LLVM_VERSION" \
        "libc++-dev~$LLVM_VERSION" \
        "llvm$llvm_major-dev~$LLVM_VERSION" \
        "llvm$llvm_major-gtest~$LLVM_VERSION" \
        "llvm$llvm_major-static~$LLVM_VERSION"; \
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
