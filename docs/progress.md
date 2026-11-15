# 进度 —— `media-libs/opus`

倒序追加，每次一个条目。门禁变更必须同时更新 `project.conf` 的 `GATE`，
否则主仓库看板会与事实脱节。

## 2026-11-13 · G0 planned

- 生成骨架（`rvv-port-project` Skill，模板来自 `libspeex-rvv`）
- 范围与通过标准写入 `docs/plan.md`
- 上游 URL：https://github.com/xiph/opus；固定点：``
- 下一步（G1）：
  1. `./scripts/fetch-upstream.sh --dry-run` 确认 URL/SHA 后再拉取
  2. `src/` 初始化为 upstream 的副本（一次性提交，便于 diff）
  3. `./scripts/build.sh riscv64` + `./test/run_ab.sh` 跑通

## 2026-11-15 G3：celt_pitch_xcorr RVV 内核

- `src/celt/riscv/pitch_xcorr_rvv.c`（由 pitch.c 同 TU include）；pitch.h 定义 OVERRIDE_PITCH_XCORR。
- 设计：int16→int32 加宽 MAC + 末尾一次归约；4 lag 共用一份 x 装载（主要收益来源）。
- 定点下要透传返回值 maxcorr，否则 pitch_search 报 void value。
- 功能：qemu off/on `fnv_bit=a04caa33e5671901` / `fnv_pcm=74bc2325f58a2e04`，与 G2 一致。
- 真机内核：len480/mp1024 5.6×、len240/mp512 5.13×、len120/mp256 4.0×。
- 真机整链：131.00 ms → 130.31 ms = **1.005×**（内核占比仅 ~1.7%，上限就到这）。
- 新坑：归约 intrinsic 一律 `vs_<源>_<目标>`（源在前），`vredsum` 同规则；内核文件要自带
  `<riscv_vector.h>`；opus 的 include 目录是 `prefix/include/opus`。
