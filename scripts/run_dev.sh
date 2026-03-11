#!/bin/bash
# 1. 授予图形界面权限
xhost +local:docker > /dev/null

# 2. 启动容器并挂载
docker run -it \
    --rm \
    --net=host \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v $(pwd):/home/VisionFlow \
    visionflow-env \
    /bin/bash
