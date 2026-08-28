ARG ALPINE_VERSION
FROM alpine:${ALPINE_VERSION}

ARG COMPILER=gcc

RUN set -eux; \
    apk --no-cache add \
        build-base \
        cmake \
        git \
        ninja-build \
        tar; \
    case "$COMPILER" in \
        gcc) ;; \
        clang) apk --no-cache add clang21 ;; \
        mingw) apk --no-cache add mingw-w64-gcc ;; \
        *) echo "Unsupported COMPILER '$COMPILER'" >&2; exit 1 ;; \
    esac; \
    rm -rf /var/cache/apk/*
