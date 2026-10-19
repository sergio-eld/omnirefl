FROM alpine:3.23.3

ARG COSMOPOLITAN_VERSION
ARG LLVM_VERSION

ENV COSMOPOLITAN_ROOT=/opt/cosmocc
ENV LLVM_SOURCE_DIR=/opt/llvm-project
ENV LLVM_BUILD_X86_64=/opt/llvm-build/x86_64
ENV LLVM_BUILD_AARCH64=/opt/llvm-build/aarch64
ENV CLANG_INSTALL_COSMOPOLITAN_X86_64=/opt/llvm-build/x86_64
ENV CLANG_INSTALL_COSMOPOLITAN_AARCH64=/opt/llvm-build/aarch64
ENV CLANG_RESOURCE_DIR_COSMOPOLITAN=/opt/llvm-build/x86_64/lib/clang/current
ENV PATH=/opt/cosmocc/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN test -n "${COSMOPOLITAN_VERSION}" \
    && test -n "${LLVM_VERSION}" \
    && apk add --no-cache \
    bash \
    build-base \
    cmake \
    curl \
    git \
    ninja-build \
    ninja-is-really-ninja \
    python3 \
    tar \
    unzip

RUN mkdir -p "${COSMOPOLITAN_ROOT}" /tmp/cosmocc \
    && curl -fL --retry 3 \
        -o /tmp/cosmocc/cosmocc.zip \
        "https://github.com/jart/cosmopolitan/releases/download/${COSMOPOLITAN_VERSION}/cosmocc-${COSMOPOLITAN_VERSION}.zip" \
    && unzip -q /tmp/cosmocc/cosmocc.zip -d "${COSMOPOLITAN_ROOT}" \
    && chmod +x "${COSMOPOLITAN_ROOT}/bin/cosmoranlib" \
    && rm -rf /tmp/cosmocc

# Alpine cannot execute APE tools directly without binfmt_misc. Convert the
# toolchain executables to native ELF while retaining APE output for products.
RUN cp "${COSMOPOLITAN_ROOT}/bin/assimilate" /tmp/assimilate.ape \
    && bash -euxo pipefail -c \
      'while IFS= read -r -d "" executable; do \
         if head -c 6 "$executable" | grep -q "^MZqFpD$"; then \
           "${COSMOPOLITAN_ROOT}/bin/ape-x86_64.elf" \
             /tmp/assimilate.ape -cex "$executable"; \
         fi; \
       done < <(find "${COSMOPOLITAN_ROOT}" -type f -perm /111 -print0)' \
    && rm /tmp/assimilate.ape

RUN git clone --depth 1 --branch "llvmorg-${LLVM_VERSION}" \
    https://github.com/llvm/llvm-project.git "${LLVM_SOURCE_DIR}"

COPY docker/llvm-project.patch /tmp/llvm-project.patch
RUN git -C "${LLVM_SOURCE_DIR}" apply /tmp/llvm-project.patch

COPY docker/cosmopolitan.cmake /opt/cosmopolitan.cmake

RUN cmake -S "${LLVM_SOURCE_DIR}/llvm" -B "${LLVM_BUILD_X86_64}" -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/cosmopolitan.cmake \
      -DCOSMOPOLITAN_ARCH=x86_64 \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_PROJECTS=clang \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF \
      -DLLVM_INCLUDE_EXAMPLES=OFF \
      -DLLVM_INCLUDE_DOCS=OFF \
      -DLLVM_ENABLE_BINDINGS=OFF \
      -DLLVM_ENABLE_ZLIB=OFF \
      -DLLVM_ENABLE_ZSTD=OFF \
      -DLLVM_ENABLE_LIBXML2=OFF \
      -DLLVM_ENABLE_CURL=OFF \
      -DLLVM_ENABLE_FFI=OFF \
      -DLLVM_ENABLE_LIBEDIT=OFF \
      -DLLVM_ENABLE_PIC=OFF \
      -DLLVM_ENABLE_LTO=OFF \
      -DCLANG_INCLUDE_TESTS=OFF \
    && cmake --build "${LLVM_BUILD_X86_64}" \
      --target clangTooling clang-resource-headers \
      --parallel "$(nproc)" \
    && llvm_major="${LLVM_VERSION%%.*}" \
    && test -d "${LLVM_BUILD_X86_64}/lib/clang/${llvm_major}" \
    && ln -s "${llvm_major}" "${CLANG_RESOURCE_DIR_COSMOPOLITAN}"

RUN cmake -S "${LLVM_SOURCE_DIR}/llvm" -B "${LLVM_BUILD_AARCH64}" -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/cosmopolitan.cmake \
      -DCOSMOPOLITAN_ARCH=aarch64 \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_HOST_TRIPLE=aarch64-unknown-linux-gnu \
      -DLLVM_DEFAULT_TARGET_TRIPLE=aarch64-unknown-linux-gnu \
      -DLLVM_ENABLE_PROJECTS=clang \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DLLVM_NATIVE_TOOL_DIR="${LLVM_BUILD_X86_64}/bin" \
      -DLLVM_TABLEGEN="${LLVM_BUILD_X86_64}/bin/llvm-tblgen" \
      -DCLANG_TABLEGEN="${LLVM_BUILD_X86_64}/bin/clang-tblgen" \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF \
      -DLLVM_INCLUDE_EXAMPLES=OFF \
      -DLLVM_INCLUDE_DOCS=OFF \
      -DLLVM_ENABLE_BINDINGS=OFF \
      -DLLVM_ENABLE_ZLIB=OFF \
      -DLLVM_ENABLE_ZSTD=OFF \
      -DLLVM_ENABLE_LIBXML2=OFF \
      -DLLVM_ENABLE_CURL=OFF \
      -DLLVM_ENABLE_FFI=OFF \
      -DLLVM_ENABLE_LIBEDIT=OFF \
      -DLLVM_ENABLE_PIC=OFF \
      -DLLVM_ENABLE_LTO=OFF \
      -DCLANG_INCLUDE_TESTS=OFF \
    && cmake --build "${LLVM_BUILD_AARCH64}" \
      --target clangTooling \
      --parallel "$(nproc)"

# The universal package uses one shared Clang resource directory. Generate the
# ARM builtin headers separately without adding an unused backend to either
# architecture's reflection tool.
RUN llvm_major="${LLVM_VERSION%%.*}" \
    && cmake -S "${LLVM_SOURCE_DIR}/llvm" \
      -B /tmp/aarch64-resource-headers \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_PROJECTS=clang \
      -DLLVM_TARGETS_TO_BUILD=AArch64 \
      -DLLVM_NATIVE_TOOL_DIR="${LLVM_BUILD_X86_64}/bin" \
      -DLLVM_TABLEGEN="${LLVM_BUILD_X86_64}/bin/llvm-tblgen" \
      -DCLANG_TABLEGEN="${LLVM_BUILD_X86_64}/bin/clang-tblgen" \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF \
      -DLLVM_INCLUDE_EXAMPLES=OFF \
      -DLLVM_INCLUDE_DOCS=OFF \
      -DLLVM_ENABLE_BINDINGS=OFF \
      -DLLVM_ENABLE_ZLIB=OFF \
      -DLLVM_ENABLE_ZSTD=OFF \
      -DLLVM_ENABLE_LIBXML2=OFF \
      -DLLVM_ENABLE_CURL=OFF \
      -DLLVM_ENABLE_FFI=OFF \
      -DLLVM_ENABLE_LIBEDIT=OFF \
      -DCLANG_INCLUDE_TESTS=OFF \
    && cmake --build /tmp/aarch64-resource-headers \
      --target clang-resource-headers \
      --parallel "$(nproc)" \
    && cp -r \
      "/tmp/aarch64-resource-headers/lib/clang/${llvm_major}/include/." \
      "${CLANG_RESOURCE_DIR_COSMOPOLITAN}/include/" \
    && test -f "${CLANG_RESOURCE_DIR_COSMOPOLITAN}/include/arm_neon.h" \
    && rm -rf /tmp/aarch64-resource-headers

RUN cmake -S "${LLVM_SOURCE_DIR}/runtimes" -B /tmp/cxx-headers -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/cosmopolitan.cmake \
      -DCOSMOPOLITAN_ARCH=x86_64 \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
      -DLIBCXX_EXTRA_SITE_DEFINES=_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE \
      -DLIBCXX_INCLUDE_TESTS=OFF \
      -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
      -DLIBCXX_ENABLE_SHARED=OFF \
      -DLIBCXX_ENABLE_STATIC=OFF \
      -DLIBCXXABI_INCLUDE_TESTS=OFF \
      -DLIBCXXABI_ENABLE_SHARED=OFF \
      -DLIBCXXABI_ENABLE_STATIC=ON \
      -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
    && cmake --build /tmp/cxx-headers \
      --target generate-cxx-headers generate-cxxabi-headers \
      --parallel "$(nproc)" \
    && resource_include="${CLANG_RESOURCE_DIR_COSMOPOLITAN}/include" \
    && mkdir -p "${resource_include}/c++" \
    && cp -r /tmp/cxx-headers/include/c++/v1 "${resource_include}/c++/" \
    && test -f "${resource_include}/c++/v1/memory" \
    && test -f "${resource_include}/c++/v1/cxxabi.h" \
    && rm -rf /tmp/cxx-headers

# Cosmopolitan 4.0.2's libcosmo.a can overrun a stack array in munmap() on
# macOS when RLIMIT_STACK is raised, as CMake does for its children. Rebuild
# only that archive member from the matching release with the fix applied,
# using its flags from libc/intrin/BUILD.mk. Products link the default-mode
# archive. This runs after LLVM to keep those layers cached; LLVM's build
# tools are never shipped.
COPY docker/cosmopolitan.patch /tmp/cosmopolitan.patch
RUN git clone --quiet --depth 1 --branch "${COSMOPOLITAN_VERSION}" \
      --filter=blob:none --sparse \
      https://github.com/jart/cosmopolitan.git /tmp/cosmopolitan \
    && git -C /tmp/cosmopolitan sparse-checkout set \
      libc ape third_party/nsync third_party/dlmalloc third_party/intel \
      third_party/aarch64 third_party/compiler_rt \
    # FIXME: [external] Remove after upstream fixes stale map tracking.
    # TODO: Add the upstream issue/reproducer link once published.
    && git -C /tmp/cosmopolitan apply /tmp/cosmopolitan.patch \
    && for arch in x86_64 aarch64; do \
         flags="-ffreestanding -fno-sanitize=all -fno-stack-protector"; \
         flags="${flags} -ffunction-sections"; \
         if [ x86_64 = "${arch}" ]; then \
           flags="${flags} -mgeneral-regs-only"; \
         fi; \
         mkdir "/tmp/cosmopolitan/${arch}" \
         && "${arch}-unknown-cosmo-cc" -std=gnu23 -O2 -g -D_COSMO_SOURCE \
           ${flags} -iquote /tmp/cosmopolitan \
           -c /tmp/cosmopolitan/libc/intrin/mmap.c \
           -o "/tmp/cosmopolitan/${arch}/mmap.o" \
         && "${arch}-unknown-cosmo-ar" rs \
           "${COSMOPOLITAN_ROOT}/${arch}-linux-cosmo/lib/libcosmo.a" \
           "/tmp/cosmopolitan/${arch}/mmap.o" \
         || exit 1; \
       done \
    && rm -rf /tmp/cosmopolitan /tmp/cosmopolitan.patch

CMD ["bash"]
