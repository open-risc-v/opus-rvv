/* opus celt_pitch_xcorr 的 RVV 定点实现。
 *
 * 被 celt/pitch.c 以同一 TU include（`-DHAVE_RVV_KERNELS=1` 且 FIXED_POINT 时），
 * 所以这里直接沿用 pitch.c 已有的 pitch.h / arch.h 定义。
 *
 * 数据形态：opus_val16（定点）= int16，opus_val32 = int32。
 * 特点：这是 pitch search 里最热的算子（上游注释原话 "the kernel you really want
 * to optimize"），每个候选 lag 都要对 len 个样点做一次内积。
 */

#include <riscv_vector.h>

/* 单 lag 内积：int16 × int16 → int32 加宽累加，末尾一次归约。
   不假设 VLEN/MVL：vl 由 vsetvl 按剩余长度给出。 */
static opus_val32 xcorr_dot_rvv( const opus_val16 * x, const opus_val16 * y,
                                 int len )
{
   size_t vlmax = __riscv_vsetvl_e16m1( (size_t)len );
   vint32m2_t acc = __riscv_vmv_v_x_i32m2( 0, vlmax );
   int j = 0;

   while( j < len ) {
      size_t vl = __riscv_vsetvl_e16m1( (size_t)( len - j ) );
      vint16m1_t vx = __riscv_vle16_v_i16m1( x + j, vl );
      vint16m1_t vy = __riscv_vle16_v_i16m1( y + j, vl );
      acc = __riscv_vwmacc_vv_i32m2( acc, vx, vy, vl );
      j += (int)vl;
   }
   {
      /* 归约 intrinsic 一律 "源在前"：vredsum_vs_<源 LMUL>_<目标>，
         vwredsumu 也是 vs_<源>_<加宽后的累加器>。写反编译器只报 implicit declaration。 */
      vint32m1_t zero = __riscv_vmv_v_x_i32m1( 0, vlmax );
      vint32m1_t s = __riscv_vredsum_vs_i32m2_i32m1( acc, zero, vlmax );
      return (opus_val32)__riscv_vmv_x_s_i32m1_i32( s );
   }
}

/* 4 个 lag 同时算，共用同一份 x 的装载 —— 与上游 xcorr_kernel 的 4-sum 结构对齐：
   每读一次 x 就摊到 4 个累加器上，装载量降到 1/4。 */
static void xcorr_dot4_rvv( const opus_val16 * x, const opus_val16 * y,
                            opus_val32 out[4], int len )
{
   size_t vlmax = __riscv_vsetvl_e16m1( (size_t)len );
   vint32m2_t a0 = __riscv_vmv_v_x_i32m2( 0, vlmax );
   vint32m2_t a1 = __riscv_vmv_v_x_i32m2( 0, vlmax );
   vint32m2_t a2 = __riscv_vmv_v_x_i32m2( 0, vlmax );
   vint32m2_t a3 = __riscv_vmv_v_x_i32m2( 0, vlmax );
   int j = 0;

   while( j < len ) {
      size_t vl = __riscv_vsetvl_e16m1( (size_t)( len - j ) );
      vint16m1_t vx = __riscv_vle16_v_i16m1( x + j, vl );
      a0 = __riscv_vwmacc_vv_i32m2( a0, vx,
                                    __riscv_vle16_v_i16m1( y + j,     vl ), vl );
      a1 = __riscv_vwmacc_vv_i32m2( a1, vx,
                                    __riscv_vle16_v_i16m1( y + j + 1, vl ), vl );
      a2 = __riscv_vwmacc_vv_i32m2( a2, vx,
                                    __riscv_vle16_v_i16m1( y + j + 2, vl ), vl );
      a3 = __riscv_vwmacc_vv_i32m2( a3, vx,
                                    __riscv_vle16_v_i16m1( y + j + 3, vl ), vl );
      j += (int)vl;
   }
   {
      vint32m1_t zero = __riscv_vmv_v_x_i32m1( 0, vlmax );
      out[0] = (opus_val32)__riscv_vmv_x_s_i32m1_i32(
                  __riscv_vredsum_vs_i32m2_i32m1( a0, zero, vlmax ) );
      out[1] = (opus_val32)__riscv_vmv_x_s_i32m1_i32(
                  __riscv_vredsum_vs_i32m2_i32m1( a1, zero, vlmax ) );
      out[2] = (opus_val32)__riscv_vmv_x_s_i32m1_i32(
                  __riscv_vredsum_vs_i32m2_i32m1( a2, zero, vlmax ) );
      out[3] = (opus_val32)__riscv_vmv_x_s_i32m1_i32(
                  __riscv_vredsum_vs_i32m2_i32m1( a3, zero, vlmax ) );
   }
}

/* 与 celt_pitch_xcorr_c 一致：定点下额外返回所有相关值的最大值（初值 1）。 */
opus_val32 celt_pitch_xcorr_rvv( const opus_val16 *_x, const opus_val16 *_y,
                                 opus_val32 *xcorr, int len, int max_pitch )
{
   int i;
   opus_val32 maxcorr = 1;

   for( i = 0; i < max_pitch - 3; i += 4 ) {
      opus_val32 s[4];
      xcorr_dot4_rvv( _x, _y + i, s, len );
      xcorr[i+0] = s[0];
      xcorr[i+1] = s[1];
      xcorr[i+2] = s[2];
      xcorr[i+3] = s[3];
      maxcorr = MAX32( maxcorr, MAX32( MAX32( s[0], s[1] ), MAX32( s[2], s[3] ) ) );
   }
   for( ; i < max_pitch; i++ ) {
      opus_val32 s = xcorr_dot_rvv( _x, _y + i, len );
      xcorr[i] = s;
      maxcorr = MAX32( maxcorr, s );
   }
   return maxcorr;
}
