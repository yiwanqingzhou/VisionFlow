#!/bin/bash
# Script to build third_party shared library

PROJECT_ROOT=$(pwd)
THIRD_PARTY_DIR="${PROJECT_ROOT}/third_party"
BUILD_DIR="${THIRD_PARTY_DIR}/build"
LIB_DIR="${PROJECT_ROOT}/lib"

mkdir -p "${LIB_DIR}"
mkdir -p "${BUILD_DIR}"

cd "${BUILD_DIR}"
cmake ..
make -j$(nproc)

echo ">>> third_party build complete. Library should be in ${LIB_DIR} <<<"
