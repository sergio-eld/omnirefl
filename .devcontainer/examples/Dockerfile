FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates cmake curl g++-14 gdb git ninja-build \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-14 CXX=g++-14
