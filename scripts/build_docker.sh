#!/bin/bash
# Rebuilds the VisionFlow Docker environment image
# Run this whenever Dockerfile is modified or dependencies are added.

echo ">>> Building Docker image: visionflow-env <<<"
docker build -t visionflow-env .

echo ">>> Done! You can now run ./scripts/run_dev.sh to enter the container. <<<"
