FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# 1. 安装系统级依赖
RUN apt-get update && apt-get install -y \
    build-essential cmake git curl wget zip unzip pkg-config \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev libglfw3-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. 设置第三方库工作目录
WORKDIR /third_party

# 3. 下载 Taskflow (Header-only)
RUN git clone --depth 1 https://github.com/taskflow/taskflow.git

# 4. 下载 nlohmann/json (Header-only)
RUN mkdir nlohmann && cd nlohmann && \
    wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp

# 5. 下载 ExprTk (Header-only)
RUN wget https://raw.githubusercontent.com/ArashPartow/exprtk/master/exprtk.hpp

# 6. 下载 ImGui 和 Node-Editor
RUN git clone --depth 1 https://github.com/ocornut/imgui.git && \
    git clone --depth 1 https://github.com/thedmd/imgui-node-editor.git

# 7. 设置项目工作目录
WORKDIR /home/TaskNodeFlow

WORKDIR /home/TaskNodeFlow

# 环境变量：让 CMake 知道库在哪里
ENV THIRD_PARTY_DIR=/third_party
