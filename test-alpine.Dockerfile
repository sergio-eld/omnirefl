FROM ghcr.io/sergio-eld/omnirefl-build-alpine:latest

RUN apk --no-cache add clang21 \
    && rm -rf /var/cache/apk/*
