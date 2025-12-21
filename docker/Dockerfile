FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    ca-certificates \
    curl \
    vim \
    cmake \
    ninja-build \
    git \
    gdb \
    clang \
    clang-format \
    pkg-config \
    python3 \
    tzdata \
    && rm -rf /var/lib/apt/lists/*

ENV TZ=Asia/Seoul

WORKDIR /workspace
