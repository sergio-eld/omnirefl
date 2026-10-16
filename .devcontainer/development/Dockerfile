# syntax=docker/dockerfile:1
FROM ghcr.io/sergio-eld/omnirefl-build-cosmo:universal-4.0.2-llvm-22.1.8 AS build-base

FROM build-base AS package

WORKDIR /src
COPY . .
# Cache compiler outputs without including the build tree in the final image.
RUN --mount=type=cache,target=/src/build,sharing=locked \
    COSMO_BUILD_UNIVERSAL=false \
    sh .devcontainer/development/package.sh /src /packages

FROM build-base

RUN apk add --no-cache gdb

COPY --from=package /packages/omnirefl.tar.gz /opt/omnirefl-packages/omnirefl.tar.gz
