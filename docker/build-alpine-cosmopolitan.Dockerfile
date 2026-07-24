FROM alpine:3.23.3

ARG COSMOPOLITAN_VERSION=4.0.2
ARG LLVM_VERSION=22.1.8

ENV COSMOPOLITAN_ROOT=/opt/cosmocc
ENV LLVM_SOURCE_DIR=/opt/llvm-project
ENV LLVM_BUILD_DIR=/opt/llvm-build
ENV CLANG_INSTALL_COSMOPOLITAN=/opt/llvm-build
ENV CLANG_RESOURCE_DIR_COSMOPOLITAN=/opt/llvm-build/lib/clang/current
ENV PATH=/opt/cosmocc/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN apk add --no-cache \
    bash \
    binutils \
    build-base \
    cmake \
    curl \
    file \
    git \
    ninja-build \
    ninja-is-really-ninja \
    python3 \
    tar \
    unzip \
    zip

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

COPY llvm-project-cosmopolitan.patch /tmp/llvm-project-cosmopolitan.patch
RUN git -C "${LLVM_SOURCE_DIR}" apply /tmp/llvm-project-cosmopolitan.patch

COPY docker/cosmopolitan.cmake /opt/cosmopolitan.cmake

RUN cmake -S "${LLVM_SOURCE_DIR}/llvm" -B "${LLVM_BUILD_DIR}" -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/cosmopolitan.cmake \
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
    && cmake --build "${LLVM_BUILD_DIR}" \
      --target clangTooling clang-resource-headers \
      --parallel "$(nproc)" \
    && llvm_major="${LLVM_VERSION%%.*}" \
    && test -d "${LLVM_BUILD_DIR}/lib/clang/${llvm_major}" \
    && ln -s "${llvm_major}" "${CLANG_RESOURCE_DIR_COSMOPOLITAN}"

COPY docker/libcxx-installed-config-site.patch /tmp/

RUN cmake -S "${LLVM_SOURCE_DIR}/runtimes" -B /tmp/cxx-headers -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/cosmopolitan.cmake \
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
    && patch --no-backup-if-mismatch \
      -d "${CLANG_RESOURCE_DIR_COSMOPOLITAN}" \
      -p1 \
      -i /tmp/libcxx-installed-config-site.patch \
    && test -f "${resource_include}/c++/v1/memory" \
    && test -f "${resource_include}/c++/v1/cxxabi.h" \
    && rm -rf \
      /tmp/cxx-headers \
      /tmp/libcxx-installed-config-site.patch

CMD ["bash"]
