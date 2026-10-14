FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates cmake curl g++-14 gdb git ninja-build \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-14 CXX=g++-14

# Use the released tool; the sandbox does not build LLVM or Omnirefl itself.
ARG OMNIREFL_VERSION=0.0.4
RUN curl -fL -o /tmp/omnirefl.deb "$(curl -fsSL \
        "https://api.github.com/repos/sergio-eld/omnirefl/releases/tags/v${OMNIREFL_VERSION}" \
        | sed -n 's/.*"browser_download_url": "\(.*linux-x86_64-musl.*\.deb\)".*/\1/p')" \
    && dpkg -i /tmp/omnirefl.deb \
    && rm /tmp/omnirefl.deb
