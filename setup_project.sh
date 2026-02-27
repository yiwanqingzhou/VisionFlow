#!/bin/bash
# 在项目根目录下执行

mkdir -p third_party/imgui/backends

# 假设你已经运行了容器，将库从容器拷贝出来或者在宿主机直接下载：
# 下面是直接下载到本地的简易命令
cd third_party
git clone --depth 1 https://github.com/ocornut/imgui.git
git clone --depth 1 https://github.com/thedmd/imgui-node-editor.git
git clone --depth 1 https://github.com/taskflow/taskflow.git
mkdir -p nlohmann && wget -O nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
wget https://raw.githubusercontent.com/ArashPartow/exprtk/master/exprtk.hpp
