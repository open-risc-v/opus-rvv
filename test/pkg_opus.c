// opus A/B 探针：固定点编码 → 解码回环。
// 判据是"解码输出位精确"：同一份 PCM 编出的码流字节与解码 PCM 都参与哈希。
#include <math.h>
#include <opus.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS 48000
#define FB 960     /* 20 ms @48k */
#define NFRAMES 120

static uint64_t fnv1a(const void *p, size_t n)
{
    const uint8_t *b = p;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}

/* 确定性语音样信号：三个正弦 + 一段噪声，量化到 16 bit */
static void fill_pcm(opus_int16 *pcm, size_t n)
{
    unsigned int s = 0x12345u;
    for (size_t i = 0; i < n; i++) {
        double t = (double)i / FS;
        double v = 0.45 * sin(2 * 3.14159265358979 * 220.0 * t)
                 + 0.25 * sin(2 * 3.14159265358979 * 660.0 * t)
                 + 0.15 * sin(2 * 3.14159265358979 * 1540.0 * t);
        s = s * 1103515245u + 12345u;
        v += ((double)((s >> 16) & 0xffff) / 65535.0 - 0.5) * 0.05;
        int iv = (int)(v * 20000.0);
        if (iv > 32767) iv = 32767;
        if (iv < -32768) iv = -32768;
        pcm[i] = (opus_int16)iv;
    }
}

int main(int argc, char **argv)
{
    const char *outp = argc > 1 ? argv[1] : NULL;
    OpusEncoder *enc = opus_encoder_create(FS, 1, OPUS_APPLICATION_AUDIO, NULL);
    OpusDecoder *dec = opus_decoder_create(FS, 1, NULL);
    if (!enc || !dec) { fprintf(stderr, "opus 初始化失败\n"); return 2; }

    opus_encoder_ctl(enc, OPUS_SET_BITRATE(64000));
    opus_encoder_ctl(enc, OPUS_SET_VBR(0));          /* CBR，去掉码率决策抖动 */
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(enc, OPUS_SET_DTX(0));
    opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(0));

    opus_int16 pcm[FB];
    opus_int16 dec_pcm[FB * 2];
    unsigned char pkt[4096];
    uint64_t hbits = 1469598103934665603ULL;
    uint64_t hpcm = 1469598103934665603ULL;
    unsigned long total_bytes = 0, total_pcm = 0;

    for (int f = 0; f < NFRAMES; f++) {
        fill_pcm(pcm, FB);
        /* 逐帧相位连续：用帧号做偏移，避免每帧完全同波形导致编码器状态退化 */
        for (int i = 0; i < FB; i++) pcm[i] = (opus_int16)(pcm[i] + ((f * 37) % 97) * 12);
        int nb = opus_encode(enc, pcm, FB, pkt, sizeof(pkt));
        if (nb < 0) { fprintf(stderr, "encode 失败: %s\n", opus_strerror(nb)); return 3; }
        total_bytes += (unsigned long)nb;
        hbits = (hbits ^ (uint64_t)nb) * 1099511628211ULL;
        for (int i = 0; i < nb; i++) { hbits ^= pkt[i]; hbits *= 1099511628211ULL; }

        int ns = opus_decode(dec, pkt, nb, dec_pcm, FB * 2, 0);
        if (ns < 0) { fprintf(stderr, "decode 失败: %s\n", opus_strerror(ns)); return 4; }
        total_pcm += (unsigned long)ns;
        for (int i = 0; i < ns; i++) { hpcm ^= (uint64_t)(uint16_t)dec_pcm[i]; hpcm *= 1099511628211ULL; }
    }

    opus_encoder_destroy(enc);
    opus_decoder_destroy(dec);

    char line[256];
    snprintf(line, sizeof line,
             "frames=%d bytes=%lu fnv_bit=%016llx pcm=%lu fnv_pcm=%016llx ver=%s\n",
             NFRAMES, total_bytes, (unsigned long long)hbits,
             total_pcm, (unsigned long long)hpcm, opus_get_version_string());

    if (outp) {
        FILE *fp = fopen(outp, "wb");
        if (!fp) { fprintf(stderr, "打不开 %s\n", outp); return 5; }
        fwrite(line, 1, strlen(line), fp);
        fclose(fp);
    }
    fputs(line, stdout);
    return 0;
}
