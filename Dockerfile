FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# 1. 安装系统级依赖
RUN apt-get update && apt-get install -y \
    build-essential cmake git curl wget zip unzip pkg-config clang-format \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev libglfw3-dev \
    graphviz libopencv-dev libpcl-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. 拷贝本地已经下载好的第三方依赖库
COPY third_party /third_party
WORKDIR /third_party

# 7. 设置项目工作目录
WORKDIR /home/VisionFlow

# 环境变量：让 CMake 知道库在哪里
ENV THIRD_PARTY_DIR=/third_party
