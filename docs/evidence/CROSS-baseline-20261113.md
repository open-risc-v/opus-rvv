# 交叉编译基线（riscv64）— opus

## 口径
- 只编上游 baseline，`MARCH=rv64gc`，`rvv-cross:1`（riscv64-linux-gnu-gcc 13.3 + qemu-riscv64）
- **固定点构建**（`OPUS_FIXED_POINT=ON`、`OPUS_DISABLE_INTRINSICS=ON`）：浮点路径会随编译器抖动，
  bit-exact 判据只有在定点下才成立。native 侧 A/B 同样定点（`cmake/pkg-build.sh` 固定）。
- 探针 `test/pkg_opus.c`：48 kHz 单声道 20 ms 帧 ×120，CBR 64 kbps、complexity 10、DTX/FEC 关；
  同时哈希码流字节与解码 PCM。

## 结果
| 侧 | 输出 |
|---|---|
| riscv64 | `frames=120 bytes=19200 fnv_bit=a04caa33e5671901 pcm=115200 fnv_pcm=74bc2325f58a2e04 ver=libopus unknown-fixed` |
| native | 同左 |

**全字段一致**（含码流字节与解码 PCM）。仍未写任何 RVV 内核，此结果只证明工具链/口径可用。

## 复现
```bash
cd ~/work/opus-rvv
./test/run_ab.sh              # native A/B
./scripts/cross-baseline.sh   # riscv64 基线
```
