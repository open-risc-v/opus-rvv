#!/usr/bin/env bash
# 构建上游 opus（CMake）→ <out>/prefix。
# 固定点构建：两侧都开 OPUS_FIXED_POINT，位精确才可比（浮点会随编译器抖动）。
set -euo pipefail
SRC=$1
OUT=$2
# EXTRA_C_FLAGS 里的空格必须整体作为一个参数传（见 libde265/x264 两次踩坑）。
EXTRA_ARGS=()
if [ -n "${EXTRA_C_FLAGS:-}" ]; then
  EXTRA_ARGS+=("-DCMAKE_C_FLAGS=${EXTRA_C_FLAGS}")
  EXTRA_ARGS+=("-DCMAKE_CXX_FLAGS=${EXTRA_C_FLAGS}")
fi

cmake -S "$SRC" -B "$OUT/bld" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$OUT/prefix" \
  -DOPUS_BUILD_SHARED_LIBRARY=OFF \
  -DOPUS_BUILD_PROGRAMS=OFF \
  -DOPUS_BUILD_TESTING=OFF \
  -DOPUS_FIXED_POINT=ON \
  -DOPUS_DISABLE_INTRINSICS=ON \
  -DOPUS_INSTALL_PKG_CONFIG_MODULE=OFF \
  -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF \
  "${EXTRA_ARGS[@]}"
cmake --build "$OUT/bld" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cmake --install "$OUT/bld"
