// opus pitch_xcorr 的内核级 A/B：同一进程内先用 C 版逐值比对，再分别计时。
// 不用内部头（pitch.h 依赖 config.h），这里直接声明两个外部符号。
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef int16_t v16;
typedef int32_t v32;

extern v32 celt_pitch_xcorr_c(const v16 *x, const v16 *y, v32 *xcorr,
                              int len, int max_pitch, int arch);
extern v32 celt_pitch_xcorr_rvv(const v16 *x, const v16 *y, v32 *xcorr,
                                int len, int max_pitch);

static uint32_t rng = 0x2468aceu;
static v16 rnd16(void)
{
    rng = rng * 1103515245u + 12345u;
    return (v16)((rng >> 16) & 0x7ff) - 1024;   /* 典型 LPC 残差量级 */
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main(int argc, char **argv)
{
    int len = argc > 1 ? atoi(argv[1]) : 480;
    int max_pitch = argc > 2 ? atoi(argv[2]) : 1024;
    int reps = argc > 3 ? atoi(argv[3]) : 2000;
    int verbose = argc > 4 && argv[4][0] == 'v';
    int i, r;
    size_t total = (size_t)len + (size_t)max_pitch + 8;

    v16 *x = malloc(total * sizeof(v16));
    v16 *y = malloc(total * sizeof(v16));
    v32 *ref = malloc((size_t)max_pitch * sizeof(v32));
    v32 *got = malloc((size_t)max_pitch * sizeof(v32));
    double t0, t1, tc = 0, tr = 0;
    v32 mc, mr;

    srand(1);
    for (i = 0; i < (int)total; i++) {
        x[i] = rnd16();
        y[i] = rnd16();
    }

    mc = celt_pitch_xcorr_c(x, y, ref, len, max_pitch, 0);
    mr = celt_pitch_xcorr_rvv(x, y, got, len, max_pitch);
    for (i = 0; i < max_pitch; i++) {
        if (ref[i] != got[i]) {
            printf("FAIL 首个失配 i=%d c=%d rvv=%d\n", i, ref[i], got[i]);
            return 1;
        }
        if (verbose && i < 6) printf("  [%d] %d\n", i, ref[i]);
    }
    if (mc != mr) {
        printf("FAIL maxcorr c=%d rvv=%d\n", mc, mr);
        return 1;
    }

    for (r = 0; r < reps; r++) {
        t0 = now_ns();
        celt_pitch_xcorr_c(x, y, ref, len, max_pitch, 0);
        t1 = now_ns();
        tc += t1 - t0;

        t0 = now_ns();
        celt_pitch_xcorr_rvv(x, y, got, len, max_pitch);
        t1 = now_ns();
        tr += t1 - t0;
    }
    printf("len=%d max_pitch=%d reps=%d  c=%.1f ns/call  rvv=%.1f ns/call  speedup=%.2fx  maxcorr=%d\n",
           len, max_pitch, reps, tc / reps, tr / reps, tc / tr, mc);
    free(x); free(y); free(ref); free(got);
    return 0;
}
