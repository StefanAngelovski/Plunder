FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    build-essential \
    ca-certificates \
    libstdc++6 \
    gcc \
    make \
    wget \
    tar \
    libssl-dev \
    file \
    curl \
    libcurl4-openssl-dev \
    libwebp-dev

WORKDIR /sdk

RUN wget -q https://github.com/trimui/toolchain_sdk_smartpro/releases/download/20231018/aarch64-linux-gnu-7.5.0-linaro.tgz && \
    tar -xzf aarch64-linux-gnu-7.5.0-linaro.tgz -C /sdk && \
    rm aarch64-linux-gnu-7.5.0-linaro.tgz

RUN wget -q https://github.com/trimui/toolchain_sdk_smartpro/releases/download/20231018/SDK_usr_tg5040_a133p.tgz && \
    tar -xzf SDK_usr_tg5040_a133p.tgz -C /sdk && \
    rm SDK_usr_tg5040_a133p.tgz

RUN wget -q https://github.com/trimui/toolchain_sdk_smartpro/releases/download/20231018/SDL2-2.26.1.GE8300.tgz && \
    tar -xzf SDL2-2.26.1.GE8300.tgz -C /sdk && \
    rm SDL2-2.26.1.GE8300.tgz

# libwebp's decoder, built for the Trimui and linked statically into Plunder:
# the device's SDL2_image (stock firmware and CrossMix) has no WebP codec, and
# most cover art from the ROM sites is WebP. Pinned version + checksum.
ARG LIBWEBP_VERSION=1.4.0
ARG LIBWEBP_SHA256=61f873ec69e3be1b99535634340d5bde750b2e4447caa1db9f61be3fd49ab1e5
RUN wget -q https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-${LIBWEBP_VERSION}.tar.gz && \
    echo "${LIBWEBP_SHA256}  libwebp-${LIBWEBP_VERSION}.tar.gz" | sha256sum -c - && \
    tar -xzf libwebp-${LIBWEBP_VERSION}.tar.gz && \
    make -C libwebp-${LIBWEBP_VERSION} -f makefile.unix -j"$(nproc)" \
        CC=/sdk/aarch64-linux-gnu-7.5.0-linaro/bin/aarch64-linux-gnu-gcc \
        AR=/sdk/aarch64-linux-gnu-7.5.0-linaro/bin/aarch64-linux-gnu-ar \
        src/libwebpdecoder.a && \
    mkdir -p /sdk/webp/include/webp /sdk/webp/lib && \
    cp libwebp-${LIBWEBP_VERSION}/src/webp/decode.h libwebp-${LIBWEBP_VERSION}/src/webp/types.h /sdk/webp/include/webp/ && \
    cp libwebp-${LIBWEBP_VERSION}/src/libwebpdecoder.a /sdk/webp/lib/ && \
    rm -rf libwebp-${LIBWEBP_VERSION} libwebp-${LIBWEBP_VERSION}.tar.gz

ENV PATH="/sdk/aarch64-linux-gnu-7.5.0-linaro/bin:$PATH"

ENV SYSROOT="/sdk/usr"
ENV SDL_DIR="/sdk/SDL2-2.26.1"

WORKDIR /app

CMD ["/bin/bash"]