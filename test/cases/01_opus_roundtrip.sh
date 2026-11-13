#!/usr/bin/env bash
# 真实包用例：opus 固定点编码 120 帧（20 ms/48 kHz）→ 解码回环，
# 同时哈希码流字节与解码 PCM。
set -euo pipefail
: "${STAGE:=run}" "${SIDE:=A}" "${BIN:=build/native}" "${DATA:=build/ab-data}" "${OUT:=build/ab-data/out}"
mkdir -p "$DATA" "$OUT"
case "$STAGE" in
  gen) ;;
  run)
    [ -x "$BIN/opus_harness" ] || { echo "缺少 $BIN/opus_harness" >&2; exit 1; }
    "$BIN/opus_harness" "$OUT/hash.txt" > /dev/null
    [ -s "$OUT/hash.txt" ] || { echo "探针没有产出" >&2; exit 1; }
    grep -q "fnv_pcm=" "$OUT/hash.txt" || { echo "缺 fnv_pcm 字段" >&2; exit 1; }
    ;;
  *) echo "未知 STAGE=$STAGE" >&2; exit 2 ;;
esac
