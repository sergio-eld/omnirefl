FROM ghcr.io/sergio-eld/omnirefl-build-alpine-x86_64-musl-ucrt:llvm-22.1.8

RUN apk --no-cache add clang21 \
    && rm -rf /var/cache/apk/*
