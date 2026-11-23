# opus G3：celt_pitch_xcorr 的 RVV 内核

- 日期：2026-11-15
- 上游：`xiph/opus` @ `0145a6248c2171bb6c4e03fb54fe016b5123e37e`
- 构建：CMake 定点（`OPUS_FIXED_POINT=ON`），两侧同为 `-march=rv64gcv`，
  唯一变量 `-DHAVE_RVV_KERNELS=1`
- 目标板：SpacemiT K3 / <distro>，`Linux k3 6.18.3 riscv64`，RVV 1.0

## 1. 选型理由

`celt_pitch_xcorr` 上游注释原话是 "the kernel you really want to optimize"，
x86（SSE/AVX2）与 ARM（NEON）都有同名参照实现，接口固定、语义单一：
`xcorr[i] = Σ_j x[j] * y[i+j]`（定点下 int16 → int32，并额外返回 maxcorr）。
是"简单项目"里最干净的一个。

## 2. 接线

- 新增 `src/celt/riscv/pitch_xcorr_rvv.c`，由 `celt/pitch.c` 在同一 TU
  `#include`（避免改 CMake 源列表）。
- `celt/pitch.h` 里定义 `OVERRIDE_PITCH_XCORR` + 宏重定向，只在
  `HAVE_RVV_KERNELS && FIXED_POINT` 时生效；浮点路径保持 C。
- 定点下 `celt_pitch_xcorr_c` **有返回值**（`maxcorr`，初值 1），宏里用逗号表达式
  让返回值透传，否则 `pitch_search` 报 "void value not ignored"。

## 3. 内核设计

- 单 lag 内积：`vwmacc_vv_i32m2`（int16 × int16 → int32 加宽累加），末尾
  `vredsum_vs_i32m2_i32m1` 一次归约，`vmv_x_s` 取标量。
- **4 个 lag 同时算、共用一份 x 装载**：与上游 `xcorr_kernel` 的 4-sum 结构对齐，
  水平装载量降到 1/4。这是本内核的主要来源（单 lag 版只有 ~2×）。
- 不假设 VLEN/MVL：`vsetvl_e16m1(len-j)` 按剩余长度给 vl，K3（VLEN=256）与
  qemu（128）都正确。

## 4. 功能验证（qemu，逐字节）

`./scripts/g3-wire-ab.sh`：

```
off: frames=120 bytes=19200 fnv_bit=a04caa33e5671901 pcm=115200 fnv_pcm=74bc2325f58a2e04
on : frames=120 bytes=19200 fnv_bit=a04caa33e5671901 pcm=115200 fnv_pcm=74bc2325f58a2e04
PASS
```

与 G2 基线哈希完全一致；`nm` 在 on 侧 `libopus.a` 里能查到
`celt_pitch_xcorr_rvv`（4 处，含调用点），说明内核确实进了二进制。

## 5. 真机性能（K3）

内核级（同一进程内先与 C 逐值比对再计时，300 次/档 × 2）：

| len | max_pitch | C | RVV | 比值 |
|---|---|---|---|---|
| 480 | 1024 | 295.9–298.8 µs | 52.5–53.3 µs | **5.60–5.64×** |
| 240 | 512 | 74.7–75.0 µs | 14.6 µs | **5.13–5.14×** |
| 120 | 256 | 18.9 µs | 4.6–4.8 µs | **3.95–4.13×** |

整链（120 帧 48 kHz 定点编码，5 轮交替、每轮内侧 3 次取最优）：

| 侧 | 5 轮（µs） |
|---|---|
| off | 130942 / 131061 / 130989 / 130991 / 131029 |
| on | 130300 / 130188 / 130295 / 130409 / 130340 |

**≈1.005×**（均值 131.00 ms → 130.31 ms，5/5 轮同向）。

## 6. 结论与归因（重要）

内核 5.6×、整链 1.005×，差距比 libde265 的 IDCT（内核 1.46× / 整链 1.004×）更极端。
原因是调用量：pitch search 走的是降采样后的 4 倍域，`len>>2 = 120`、
`max_pitch>>2 = 256`，每次调用 C 版只要 ~19 µs，一帧一次左右 ——
120 帧总共约 2.3 ms，占 131 ms 的 1.7%，而内核只能吃掉这部分里的 80%，
上限就是 ~1.4%，实测 0.5%（有余下的调用量波动与缓存效应）。

**这条再次印证：整链增益 ≈ 内核倍数 × 该内核在管线里的时间占比。**
opus 真正的时间大头是 MDCT/FFT（`clt_mdct_forward/backward`）与 `comb_filter`，
它们不是"简单内核"，所以本轮到此为止。

## 7. 尚未做

- `xcorr_kernel`（`celt_lpc.c` 的自相关，`len = ord` 只有 16–24）未向量化：
  尊重这条"宽度决定收益"的老结论，小 ord 上向量化基本无收益。
- `dual_inner_prod`、`celt_inner_prod` 未做（同样小 len）。
- MDCT/FFT 未做（真正的热点，但属于中等以上难度）。
- 无运行时 CPU 探测，编译期宏。

## 8. 复现

- 功能：`./scripts/g3-wire-ab.sh`
- 内核：`/root/rvv-k/opus/kernel_xcorr <len> <max_pitch> <reps> [v]`
- 整链：`/root/rvv-k/opus/enc_off|enc_on`
