# VisionFlow

VisionFlow 是一个基于 C++ 的节点流式处理框架，提供了高性能的核心引擎以及基于 ImGui 的可视化交互界面。

## 快速开始

本项目完全在 Docker 容器内构建和运行，以确保依赖环境的一致性。

### 1. 构建环境镜像
在宿主机项目根目录下运行脚本，构建名为 `visionflow-env` 的 Docker 镜像：
```bash
./scripts/build_docker.sh
```

### 2. 启动并进入容器
运行开发容器。该脚本会自动处理 `DISPLAY` 环境变量挂载，以便在容器内运行 GUI 程序：
```bash
./scripts/run_dev.sh
```
*注意：进入容器后，你将处于 `/home/VisionFlow` 目录下。*

---

### 3. 项目编译

本项目支持 **二进制化模式**（为了保护或剥离第三方源码）和 **源码编译模式**。

#### A. 生成第三方预编译库 (仅需执行一次)
如果您是初次运行且 `lib/` 目录下没有 `.so` 文件，请先在容器内生成它：
```bash
./scripts/build_third_party.sh
```
这会在 `lib/` 目录下生成 `libvf_third_party.so`。

#### B. 编译主项目

**链接预编译库 (推荐):**
```bash
mkdir -p build && cd build
cmake .. -DVF_USE_PREBUILT_THIRD_PARTY=ON
make -j$(nproc)
```

---

### 4. 运行程序

编译成功后，可执行文件位于 `build/ui/VisionFlow`：
```bash
./ui/VisionFlow
```
