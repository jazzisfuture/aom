/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/common/x86/av1_inv_txfm_avx2.h"
#include "av1/common/x86/av1_inv_txfm_ssse3.h"

static void idct16_new_avx2(const __m256i *input, __m256i *output,
                            int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i __rounding = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
  __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
  __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
  __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
  __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
  __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
  __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);
  __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
  __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x1[16];
  x1[0] = input[0];
  x1[1] = input[8];
  x1[2] = input[4];
  x1[3] = input[12];
  x1[4] = input[2];
  x1[5] = input[10];
  x1[6] = input[6];
  x1[7] = input[14];
  x1[8] = input[1];
  x1[9] = input[9];
  x1[10] = input[5];
  x1[11] = input[13];
  x1[12] = input[3];
  x1[13] = input[11];
  x1[14] = input[7];
  x1[15] = input[15];

  // stage 2
  __m256i x2[16];
  x2[0] = x1[0];
  x2[1] = x1[1];
  x2[2] = x1[2];
  x2[3] = x1[3];
  x2[4] = x1[4];
  x2[5] = x1[5];
  x2[6] = x1[6];
  x2[7] = x1[7];
  btf_16_w16_avx2(cospi_p60_m04, cospi_p04_p60, x1[8], x1[15], x2[8], x2[15]);
  btf_16_w16_avx2(cospi_p28_m36, cospi_p36_p28, x1[9], x1[14], x2[9], x2[14]);
  btf_16_w16_avx2(cospi_p44_m20, cospi_p20_p44, x1[10], x1[13], x2[10], x2[13]);
  btf_16_w16_avx2(cospi_p12_m52, cospi_p52_p12, x1[11], x1[12], x2[11], x2[12]);

  // stage 3
  __m256i x3[16];
  x3[0] = x2[0];
  x3[1] = x2[1];
  x3[2] = x2[2];
  x3[3] = x2[3];
  btf_16_w16_avx2(cospi_p56_m08, cospi_p08_p56, x2[4], x2[7], x3[4], x3[7]);
  btf_16_w16_avx2(cospi_p24_m40, cospi_p40_p24, x2[5], x2[6], x3[5], x3[6]);
  x3[8] = _mm256_adds_epi16(x2[8], x2[9]);
  x3[9] = _mm256_subs_epi16(x2[8], x2[9]);
  x3[10] = _mm256_subs_epi16(x2[11], x2[10]);
  x3[11] = _mm256_adds_epi16(x2[10], x2[11]);
  x3[12] = _mm256_adds_epi16(x2[12], x2[13]);
  x3[13] = _mm256_subs_epi16(x2[12], x2[13]);
  x3[14] = _mm256_subs_epi16(x2[15], x2[14]);
  x3[15] = _mm256_adds_epi16(x2[14], x2[15]);

  // stage 4
  __m256i x4[16];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x3[0], x3[1], x4[0], x4[1]);
  btf_16_w16_avx2(cospi_p48_m16, cospi_p16_p48, x3[2], x3[3], x4[2], x4[3]);
  x4[4] = _mm256_adds_epi16(x3[4], x3[5]);
  x4[5] = _mm256_subs_epi16(x3[4], x3[5]);
  x4[6] = _mm256_subs_epi16(x3[7], x3[6]);
  x4[7] = _mm256_adds_epi16(x3[6], x3[7]);
  x4[8] = x3[8];
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x3[9], x3[14], x4[9], x4[14]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x3[10], x3[13], x4[10], x4[13]);
  x4[11] = x3[11];
  x4[12] = x3[12];
  x4[15] = x3[15];

  // stage 5
  __m256i x5[16];
  x5[0] = _mm256_adds_epi16(x4[0], x4[3]);
  x5[3] = _mm256_subs_epi16(x4[0], x4[3]);
  x5[1] = _mm256_adds_epi16(x4[1], x4[2]);
  x5[2] = _mm256_subs_epi16(x4[1], x4[2]);
  x5[4] = x4[4];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x4[5], x4[6], x5[5], x5[6]);
  x5[7] = x4[7];
  x5[8] = _mm256_adds_epi16(x4[8], x4[11]);
  x5[11] = _mm256_subs_epi16(x4[8], x4[11]);
  x5[9] = _mm256_adds_epi16(x4[9], x4[10]);
  x5[10] = _mm256_subs_epi16(x4[9], x4[10]);
  x5[12] = _mm256_subs_epi16(x4[15], x4[12]);
  x5[15] = _mm256_adds_epi16(x4[12], x4[15]);
  x5[13] = _mm256_subs_epi16(x4[14], x4[13]);
  x5[14] = _mm256_adds_epi16(x4[13], x4[14]);

  // stage 6
  __m256i x6[16];
  x6[0] = _mm256_adds_epi16(x5[0], x5[7]);
  x6[7] = _mm256_subs_epi16(x5[0], x5[7]);
  x6[1] = _mm256_adds_epi16(x5[1], x5[6]);
  x6[6] = _mm256_subs_epi16(x5[1], x5[6]);
  x6[2] = _mm256_adds_epi16(x5[2], x5[5]);
  x6[5] = _mm256_subs_epi16(x5[2], x5[5]);
  x6[3] = _mm256_adds_epi16(x5[3], x5[4]);
  x6[4] = _mm256_subs_epi16(x5[3], x5[4]);
  x6[8] = x5[8];
  x6[9] = x5[9];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x5[10], x5[13], x6[10], x6[13]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x5[11], x5[12], x6[11], x6[12]);
  x6[14] = x5[14];
  x6[15] = x5[15];

  // stage 7
  output[0] = _mm256_adds_epi16(x6[0], x6[15]);
  output[15] = _mm256_subs_epi16(x6[0], x6[15]);
  output[1] = _mm256_adds_epi16(x6[1], x6[14]);
  output[14] = _mm256_subs_epi16(x6[1], x6[14]);
  output[2] = _mm256_adds_epi16(x6[2], x6[13]);
  output[13] = _mm256_subs_epi16(x6[2], x6[13]);
  output[3] = _mm256_adds_epi16(x6[3], x6[12]);
  output[12] = _mm256_subs_epi16(x6[3], x6[12]);
  output[4] = _mm256_adds_epi16(x6[4], x6[11]);
  output[11] = _mm256_subs_epi16(x6[4], x6[11]);
  output[5] = _mm256_adds_epi16(x6[5], x6[10]);
  output[10] = _mm256_subs_epi16(x6[5], x6[10]);
  output[6] = _mm256_adds_epi16(x6[6], x6[9]);
  output[9] = _mm256_subs_epi16(x6[6], x6[9]);
  output[7] = _mm256_adds_epi16(x6[7], x6[8]);
  output[8] = _mm256_subs_epi16(x6[7], x6[8]);
}

static void iadst16_new_avx2(const __m256i *input, __m256i *output,
                             int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i __zero = _mm256_setzero_si256();
  const __m256i __rounding = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p02_p62 = pair_set_w16_epi16(cospi[2], cospi[62]);
  __m256i cospi_p62_m02 = pair_set_w16_epi16(cospi[62], -cospi[2]);
  __m256i cospi_p10_p54 = pair_set_w16_epi16(cospi[10], cospi[54]);
  __m256i cospi_p54_m10 = pair_set_w16_epi16(cospi[54], -cospi[10]);
  __m256i cospi_p18_p46 = pair_set_w16_epi16(cospi[18], cospi[46]);
  __m256i cospi_p46_m18 = pair_set_w16_epi16(cospi[46], -cospi[18]);
  __m256i cospi_p26_p38 = pair_set_w16_epi16(cospi[26], cospi[38]);
  __m256i cospi_p38_m26 = pair_set_w16_epi16(cospi[38], -cospi[26]);
  __m256i cospi_p34_p30 = pair_set_w16_epi16(cospi[34], cospi[30]);
  __m256i cospi_p30_m34 = pair_set_w16_epi16(cospi[30], -cospi[34]);
  __m256i cospi_p42_p22 = pair_set_w16_epi16(cospi[42], cospi[22]);
  __m256i cospi_p22_m42 = pair_set_w16_epi16(cospi[22], -cospi[42]);
  __m256i cospi_p50_p14 = pair_set_w16_epi16(cospi[50], cospi[14]);
  __m256i cospi_p14_m50 = pair_set_w16_epi16(cospi[14], -cospi[50]);
  __m256i cospi_p58_p06 = pair_set_w16_epi16(cospi[58], cospi[6]);
  __m256i cospi_p06_m58 = pair_set_w16_epi16(cospi[6], -cospi[58]);
  __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  __m256i cospi_m56_p08 = pair_set_w16_epi16(-cospi[56], cospi[8]);
  __m256i cospi_m24_p40 = pair_set_w16_epi16(-cospi[24], cospi[40]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);

  // stage 1
  __m256i x1[16];
  x1[0] = input[15];
  x1[1] = input[0];
  x1[2] = input[13];
  x1[3] = input[2];
  x1[4] = input[11];
  x1[5] = input[4];
  x1[6] = input[9];
  x1[7] = input[6];
  x1[8] = input[7];
  x1[9] = input[8];
  x1[10] = input[5];
  x1[11] = input[10];
  x1[12] = input[3];
  x1[13] = input[12];
  x1[14] = input[1];
  x1[15] = input[14];

  // stage 2
  __m256i x2[16];
  btf_16_w16_avx2(cospi_p02_p62, cospi_p62_m02, x1[0], x1[1], x2[0], x2[1]);
  btf_16_w16_avx2(cospi_p10_p54, cospi_p54_m10, x1[2], x1[3], x2[2], x2[3]);
  btf_16_w16_avx2(cospi_p18_p46, cospi_p46_m18, x1[4], x1[5], x2[4], x2[5]);
  btf_16_w16_avx2(cospi_p26_p38, cospi_p38_m26, x1[6], x1[7], x2[6], x2[7]);
  btf_16_w16_avx2(cospi_p34_p30, cospi_p30_m34, x1[8], x1[9], x2[8], x2[9]);
  btf_16_w16_avx2(cospi_p42_p22, cospi_p22_m42, x1[10], x1[11], x2[10], x2[11]);
  btf_16_w16_avx2(cospi_p50_p14, cospi_p14_m50, x1[12], x1[13], x2[12], x2[13]);
  btf_16_w16_avx2(cospi_p58_p06, cospi_p06_m58, x1[14], x1[15], x2[14], x2[15]);

  // stage 3
  __m256i x3[16];
  x3[0] = _mm256_adds_epi16(x2[0], x2[8]);
  x3[8] = _mm256_subs_epi16(x2[0], x2[8]);
  x3[1] = _mm256_adds_epi16(x2[1], x2[9]);
  x3[9] = _mm256_subs_epi16(x2[1], x2[9]);
  x3[2] = _mm256_adds_epi16(x2[2], x2[10]);
  x3[10] = _mm256_subs_epi16(x2[2], x2[10]);
  x3[3] = _mm256_adds_epi16(x2[3], x2[11]);
  x3[11] = _mm256_subs_epi16(x2[3], x2[11]);
  x3[4] = _mm256_adds_epi16(x2[4], x2[12]);
  x3[12] = _mm256_subs_epi16(x2[4], x2[12]);
  x3[5] = _mm256_adds_epi16(x2[5], x2[13]);
  x3[13] = _mm256_subs_epi16(x2[5], x2[13]);
  x3[6] = _mm256_adds_epi16(x2[6], x2[14]);
  x3[14] = _mm256_subs_epi16(x2[6], x2[14]);
  x3[7] = _mm256_adds_epi16(x2[7], x2[15]);
  x3[15] = _mm256_subs_epi16(x2[7], x2[15]);

  // stage 4
  __m256i x4[16];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  x4[4] = x3[4];
  x4[5] = x3[5];
  x4[6] = x3[6];
  x4[7] = x3[7];
  btf_16_w16_avx2(cospi_p08_p56, cospi_p56_m08, x3[8], x3[9], x4[8], x4[9]);
  btf_16_w16_avx2(cospi_p40_p24, cospi_p24_m40, x3[10], x3[11], x4[10], x4[11]);
  btf_16_w16_avx2(cospi_m56_p08, cospi_p08_p56, x3[12], x3[13], x4[12], x4[13]);
  btf_16_w16_avx2(cospi_m24_p40, cospi_p40_p24, x3[14], x3[15], x4[14], x4[15]);

  // stage 5
  __m256i x5[16];
  x5[0] = _mm256_adds_epi16(x4[0], x4[4]);
  x5[4] = _mm256_subs_epi16(x4[0], x4[4]);
  x5[1] = _mm256_adds_epi16(x4[1], x4[5]);
  x5[5] = _mm256_subs_epi16(x4[1], x4[5]);
  x5[2] = _mm256_adds_epi16(x4[2], x4[6]);
  x5[6] = _mm256_subs_epi16(x4[2], x4[6]);
  x5[3] = _mm256_adds_epi16(x4[3], x4[7]);
  x5[7] = _mm256_subs_epi16(x4[3], x4[7]);
  x5[8] = _mm256_adds_epi16(x4[8], x4[12]);
  x5[12] = _mm256_subs_epi16(x4[8], x4[12]);
  x5[9] = _mm256_adds_epi16(x4[9], x4[13]);
  x5[13] = _mm256_subs_epi16(x4[9], x4[13]);
  x5[10] = _mm256_adds_epi16(x4[10], x4[14]);
  x5[14] = _mm256_subs_epi16(x4[10], x4[14]);
  x5[11] = _mm256_adds_epi16(x4[11], x4[15]);
  x5[15] = _mm256_subs_epi16(x4[11], x4[15]);

  // stage 6
  __m256i x6[16];
  x6[0] = x5[0];
  x6[1] = x5[1];
  x6[2] = x5[2];
  x6[3] = x5[3];
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, x5[4], x5[5], x6[4], x6[5]);
  btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, x5[6], x5[7], x6[6], x6[7]);
  x6[8] = x5[8];
  x6[9] = x5[9];
  x6[10] = x5[10];
  x6[11] = x5[11];
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, x5[12], x5[13], x6[12], x6[13]);
  btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, x5[14], x5[15], x6[14], x6[15]);

  // stage 7
  __m256i x7[16];
  x7[0] = _mm256_adds_epi16(x6[0], x6[2]);
  x7[2] = _mm256_subs_epi16(x6[0], x6[2]);
  x7[1] = _mm256_adds_epi16(x6[1], x6[3]);
  x7[3] = _mm256_subs_epi16(x6[1], x6[3]);
  x7[4] = _mm256_adds_epi16(x6[4], x6[6]);
  x7[6] = _mm256_subs_epi16(x6[4], x6[6]);
  x7[5] = _mm256_adds_epi16(x6[5], x6[7]);
  x7[7] = _mm256_subs_epi16(x6[5], x6[7]);
  x7[8] = _mm256_adds_epi16(x6[8], x6[10]);
  x7[10] = _mm256_subs_epi16(x6[8], x6[10]);
  x7[9] = _mm256_adds_epi16(x6[9], x6[11]);
  x7[11] = _mm256_subs_epi16(x6[9], x6[11]);
  x7[12] = _mm256_adds_epi16(x6[12], x6[14]);
  x7[14] = _mm256_subs_epi16(x6[12], x6[14]);
  x7[13] = _mm256_adds_epi16(x6[13], x6[15]);
  x7[15] = _mm256_subs_epi16(x6[13], x6[15]);

  // stage 8
  __m256i x8[16];
  x8[0] = x7[0];
  x8[1] = x7[1];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x7[2], x7[3], x8[2], x8[3]);
  x8[4] = x7[4];
  x8[5] = x7[5];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x7[6], x7[7], x8[6], x8[7]);
  x8[8] = x7[8];
  x8[9] = x7[9];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x7[10], x7[11], x8[10], x8[11]);
  x8[12] = x7[12];
  x8[13] = x7[13];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x7[14], x7[15], x8[14], x8[15]);

  // stage 9
  output[0] = x8[0];
  output[1] = _mm256_subs_epi16(__zero, x8[8]);
  output[2] = x8[12];
  output[3] = _mm256_subs_epi16(__zero, x8[4]);
  output[4] = x8[6];
  output[5] = _mm256_subs_epi16(__zero, x8[14]);
  output[6] = x8[10];
  output[7] = _mm256_subs_epi16(__zero, x8[2]);
  output[8] = x8[3];
  output[9] = _mm256_subs_epi16(__zero, x8[11]);
  output[10] = x8[15];
  output[11] = _mm256_subs_epi16(__zero, x8[7]);
  output[12] = x8[5];
  output[13] = _mm256_subs_epi16(__zero, x8[13]);
  output[14] = x8[9];
  output[15] = _mm256_subs_epi16(__zero, x8[1]);
}

static void iidentity16_new_avx2(const __m256i *input, __m256i *output,
                                 int8_t cos_bit) {
  (void)cos_bit;
  const int16_t scale_fractional = 2 * (NewSqrt2 - (1 << NewSqrt2Bits));
  const __m256i scale =
      _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  for (int i = 0; i < 16; ++i) {
    __m256i x = _mm256_mulhrs_epi16(input[i], scale);
    __m256i srcx2 = _mm256_adds_epi16(input[i], input[i]);
    output[i] = _mm256_adds_epi16(x, srcx2);
  }
}

void idct32_new_avx2(const __m256i *input, __m256i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i __rounding = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p62_m02 = pair_set_w16_epi16(cospi[62], -cospi[2]);
  __m256i cospi_p02_p62 = pair_set_w16_epi16(cospi[2], cospi[62]);
  __m256i cospi_p30_m34 = pair_set_w16_epi16(cospi[30], -cospi[34]);
  __m256i cospi_p34_p30 = pair_set_w16_epi16(cospi[34], cospi[30]);
  __m256i cospi_p46_m18 = pair_set_w16_epi16(cospi[46], -cospi[18]);
  __m256i cospi_p18_p46 = pair_set_w16_epi16(cospi[18], cospi[46]);
  __m256i cospi_p14_m50 = pair_set_w16_epi16(cospi[14], -cospi[50]);
  __m256i cospi_p50_p14 = pair_set_w16_epi16(cospi[50], cospi[14]);
  __m256i cospi_p54_m10 = pair_set_w16_epi16(cospi[54], -cospi[10]);
  __m256i cospi_p10_p54 = pair_set_w16_epi16(cospi[10], cospi[54]);
  __m256i cospi_p22_m42 = pair_set_w16_epi16(cospi[22], -cospi[42]);
  __m256i cospi_p42_p22 = pair_set_w16_epi16(cospi[42], cospi[22]);
  __m256i cospi_p38_m26 = pair_set_w16_epi16(cospi[38], -cospi[26]);
  __m256i cospi_p26_p38 = pair_set_w16_epi16(cospi[26], cospi[38]);
  __m256i cospi_p06_m58 = pair_set_w16_epi16(cospi[6], -cospi[58]);
  __m256i cospi_p58_p06 = pair_set_w16_epi16(cospi[58], cospi[6]);
  __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
  __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
  __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
  __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
  __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
  __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
  __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);
  __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
  __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x1[32];
  x1[0] = input[0];
  x1[1] = input[16];
  x1[2] = input[8];
  x1[3] = input[24];
  x1[4] = input[4];
  x1[5] = input[20];
  x1[6] = input[12];
  x1[7] = input[28];
  x1[8] = input[2];
  x1[9] = input[18];
  x1[10] = input[10];
  x1[11] = input[26];
  x1[12] = input[6];
  x1[13] = input[22];
  x1[14] = input[14];
  x1[15] = input[30];
  x1[16] = input[1];
  x1[17] = input[17];
  x1[18] = input[9];
  x1[19] = input[25];
  x1[20] = input[5];
  x1[21] = input[21];
  x1[22] = input[13];
  x1[23] = input[29];
  x1[24] = input[3];
  x1[25] = input[19];
  x1[26] = input[11];
  x1[27] = input[27];
  x1[28] = input[7];
  x1[29] = input[23];
  x1[30] = input[15];
  x1[31] = input[31];

  // stage 2
  __m256i x2[32];
  x2[0] = x1[0];
  x2[1] = x1[1];
  x2[2] = x1[2];
  x2[3] = x1[3];
  x2[4] = x1[4];
  x2[5] = x1[5];
  x2[6] = x1[6];
  x2[7] = x1[7];
  x2[8] = x1[8];
  x2[9] = x1[9];
  x2[10] = x1[10];
  x2[11] = x1[11];
  x2[12] = x1[12];
  x2[13] = x1[13];
  x2[14] = x1[14];
  x2[15] = x1[15];
  btf_16_w16_avx2(cospi_p62_m02, cospi_p02_p62, x1[16], x1[31], x2[16], x2[31]);
  btf_16_w16_avx2(cospi_p30_m34, cospi_p34_p30, x1[17], x1[30], x2[17], x2[30]);
  btf_16_w16_avx2(cospi_p46_m18, cospi_p18_p46, x1[18], x1[29], x2[18], x2[29]);
  btf_16_w16_avx2(cospi_p14_m50, cospi_p50_p14, x1[19], x1[28], x2[19], x2[28]);
  btf_16_w16_avx2(cospi_p54_m10, cospi_p10_p54, x1[20], x1[27], x2[20], x2[27]);
  btf_16_w16_avx2(cospi_p22_m42, cospi_p42_p22, x1[21], x1[26], x2[21], x2[26]);
  btf_16_w16_avx2(cospi_p38_m26, cospi_p26_p38, x1[22], x1[25], x2[22], x2[25]);
  btf_16_w16_avx2(cospi_p06_m58, cospi_p58_p06, x1[23], x1[24], x2[23], x2[24]);

  // stage 3
  __m256i x3[32];
  x3[0] = x2[0];
  x3[1] = x2[1];
  x3[2] = x2[2];
  x3[3] = x2[3];
  x3[4] = x2[4];
  x3[5] = x2[5];
  x3[6] = x2[6];
  x3[7] = x2[7];
  btf_16_w16_avx2(cospi_p60_m04, cospi_p04_p60, x2[8], x2[15], x3[8], x3[15]);
  btf_16_w16_avx2(cospi_p28_m36, cospi_p36_p28, x2[9], x2[14], x3[9], x3[14]);
  btf_16_w16_avx2(cospi_p44_m20, cospi_p20_p44, x2[10], x2[13], x3[10], x3[13]);
  btf_16_w16_avx2(cospi_p12_m52, cospi_p52_p12, x2[11], x2[12], x3[11], x3[12]);
  x3[16] = _mm256_adds_epi16(x2[16], x2[17]);
  x3[17] = _mm256_subs_epi16(x2[16], x2[17]);
  x3[18] = _mm256_subs_epi16(x2[19], x2[18]);
  x3[19] = _mm256_adds_epi16(x2[18], x2[19]);
  x3[20] = _mm256_adds_epi16(x2[20], x2[21]);
  x3[21] = _mm256_subs_epi16(x2[20], x2[21]);
  x3[22] = _mm256_subs_epi16(x2[23], x2[22]);
  x3[23] = _mm256_adds_epi16(x2[22], x2[23]);
  x3[24] = _mm256_adds_epi16(x2[24], x2[25]);
  x3[25] = _mm256_subs_epi16(x2[24], x2[25]);
  x3[26] = _mm256_subs_epi16(x2[27], x2[26]);
  x3[27] = _mm256_adds_epi16(x2[26], x2[27]);
  x3[28] = _mm256_adds_epi16(x2[28], x2[29]);
  x3[29] = _mm256_subs_epi16(x2[28], x2[29]);
  x3[30] = _mm256_subs_epi16(x2[31], x2[30]);
  x3[31] = _mm256_adds_epi16(x2[30], x2[31]);

  // stage 4
  __m256i x4[32];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_w16_avx2(cospi_p56_m08, cospi_p08_p56, x3[4], x3[7], x4[4], x4[7]);
  btf_16_w16_avx2(cospi_p24_m40, cospi_p40_p24, x3[5], x3[6], x4[5], x4[6]);
  x4[8] = _mm256_adds_epi16(x3[8], x3[9]);
  x4[9] = _mm256_subs_epi16(x3[8], x3[9]);
  x4[10] = _mm256_subs_epi16(x3[11], x3[10]);
  x4[11] = _mm256_adds_epi16(x3[10], x3[11]);
  x4[12] = _mm256_adds_epi16(x3[12], x3[13]);
  x4[13] = _mm256_subs_epi16(x3[12], x3[13]);
  x4[14] = _mm256_subs_epi16(x3[15], x3[14]);
  x4[15] = _mm256_adds_epi16(x3[14], x3[15]);
  x4[16] = x3[16];
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, x3[17], x3[30], x4[17], x4[30]);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, x3[18], x3[29], x4[18], x4[29]);
  x4[19] = x3[19];
  x4[20] = x3[20];
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, x3[21], x3[26], x4[21], x4[26]);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, x3[22], x3[25], x4[22], x4[25]);
  x4[23] = x3[23];
  x4[24] = x3[24];
  x4[27] = x3[27];
  x4[28] = x3[28];
  x4[31] = x3[31];

  // stage 5
  __m256i x5[32];
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, x4[0], x4[1], x5[0], x5[1]);
  btf_16_w16_avx2(cospi_p48_m16, cospi_p16_p48, x4[2], x4[3], x5[2], x5[3]);
  x5[4] = _mm256_adds_epi16(x4[4], x4[5]);
  x5[5] = _mm256_subs_epi16(x4[4], x4[5]);
  x5[6] = _mm256_subs_epi16(x4[7], x4[6]);
  x5[7] = _mm256_adds_epi16(x4[6], x4[7]);
  x5[8] = x4[8];
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x4[9], x4[14], x5[9], x5[14]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x4[10], x4[13], x5[10], x5[13]);
  x5[11] = x4[11];
  x5[12] = x4[12];
  x5[15] = x4[15];
  x5[16] = _mm256_adds_epi16(x4[16], x4[19]);
  x5[19] = _mm256_subs_epi16(x4[16], x4[19]);
  x5[17] = _mm256_adds_epi16(x4[17], x4[18]);
  x5[18] = _mm256_subs_epi16(x4[17], x4[18]);
  x5[20] = _mm256_subs_epi16(x4[23], x4[20]);
  x5[23] = _mm256_adds_epi16(x4[20], x4[23]);
  x5[21] = _mm256_subs_epi16(x4[22], x4[21]);
  x5[22] = _mm256_adds_epi16(x4[21], x4[22]);
  x5[24] = _mm256_adds_epi16(x4[24], x4[27]);
  x5[27] = _mm256_subs_epi16(x4[24], x4[27]);
  x5[25] = _mm256_adds_epi16(x4[25], x4[26]);
  x5[26] = _mm256_subs_epi16(x4[25], x4[26]);
  x5[28] = _mm256_subs_epi16(x4[31], x4[28]);
  x5[31] = _mm256_adds_epi16(x4[28], x4[31]);
  x5[29] = _mm256_subs_epi16(x4[30], x4[29]);
  x5[30] = _mm256_adds_epi16(x4[29], x4[30]);

  // stage 6
  __m256i x6[32];
  x6[0] = _mm256_adds_epi16(x5[0], x5[3]);
  x6[3] = _mm256_subs_epi16(x5[0], x5[3]);
  x6[1] = _mm256_adds_epi16(x5[1], x5[2]);
  x6[2] = _mm256_subs_epi16(x5[1], x5[2]);
  x6[4] = x5[4];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x5[5], x5[6], x6[5], x6[6]);
  x6[7] = x5[7];
  x6[8] = _mm256_adds_epi16(x5[8], x5[11]);
  x6[11] = _mm256_subs_epi16(x5[8], x5[11]);
  x6[9] = _mm256_adds_epi16(x5[9], x5[10]);
  x6[10] = _mm256_subs_epi16(x5[9], x5[10]);
  x6[12] = _mm256_subs_epi16(x5[15], x5[12]);
  x6[15] = _mm256_adds_epi16(x5[12], x5[15]);
  x6[13] = _mm256_subs_epi16(x5[14], x5[13]);
  x6[14] = _mm256_adds_epi16(x5[13], x5[14]);
  x6[16] = x5[16];
  x6[17] = x5[17];
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x5[18], x5[29], x6[18], x6[29]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x5[19], x5[28], x6[19], x6[28]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x5[20], x5[27], x6[20], x6[27]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x5[21], x5[26], x6[21], x6[26]);
  x6[22] = x5[22];
  x6[23] = x5[23];
  x6[24] = x5[24];
  x6[25] = x5[25];
  x6[30] = x5[30];
  x6[31] = x5[31];

  // stage 7
  __m256i x7[32];
  x7[0] = _mm256_adds_epi16(x6[0], x6[7]);
  x7[7] = _mm256_subs_epi16(x6[0], x6[7]);
  x7[1] = _mm256_adds_epi16(x6[1], x6[6]);
  x7[6] = _mm256_subs_epi16(x6[1], x6[6]);
  x7[2] = _mm256_adds_epi16(x6[2], x6[5]);
  x7[5] = _mm256_subs_epi16(x6[2], x6[5]);
  x7[3] = _mm256_adds_epi16(x6[3], x6[4]);
  x7[4] = _mm256_subs_epi16(x6[3], x6[4]);
  x7[8] = x6[8];
  x7[9] = x6[9];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x6[10], x6[13], x7[10], x7[13]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x6[11], x6[12], x7[11], x7[12]);
  x7[14] = x6[14];
  x7[15] = x6[15];
  x7[16] = _mm256_adds_epi16(x6[16], x6[23]);
  x7[23] = _mm256_subs_epi16(x6[16], x6[23]);
  x7[17] = _mm256_adds_epi16(x6[17], x6[22]);
  x7[22] = _mm256_subs_epi16(x6[17], x6[22]);
  x7[18] = _mm256_adds_epi16(x6[18], x6[21]);
  x7[21] = _mm256_subs_epi16(x6[18], x6[21]);
  x7[19] = _mm256_adds_epi16(x6[19], x6[20]);
  x7[20] = _mm256_subs_epi16(x6[19], x6[20]);
  x7[24] = _mm256_subs_epi16(x6[31], x6[24]);
  x7[31] = _mm256_adds_epi16(x6[24], x6[31]);
  x7[25] = _mm256_subs_epi16(x6[30], x6[25]);
  x7[30] = _mm256_adds_epi16(x6[25], x6[30]);
  x7[26] = _mm256_subs_epi16(x6[29], x6[26]);
  x7[29] = _mm256_adds_epi16(x6[26], x6[29]);
  x7[27] = _mm256_subs_epi16(x6[28], x6[27]);
  x7[28] = _mm256_adds_epi16(x6[27], x6[28]);

  // stage 8
  __m256i x8[32];
  x8[0] = _mm256_adds_epi16(x7[0], x7[15]);
  x8[15] = _mm256_subs_epi16(x7[0], x7[15]);
  x8[1] = _mm256_adds_epi16(x7[1], x7[14]);
  x8[14] = _mm256_subs_epi16(x7[1], x7[14]);
  x8[2] = _mm256_adds_epi16(x7[2], x7[13]);
  x8[13] = _mm256_subs_epi16(x7[2], x7[13]);
  x8[3] = _mm256_adds_epi16(x7[3], x7[12]);
  x8[12] = _mm256_subs_epi16(x7[3], x7[12]);
  x8[4] = _mm256_adds_epi16(x7[4], x7[11]);
  x8[11] = _mm256_subs_epi16(x7[4], x7[11]);
  x8[5] = _mm256_adds_epi16(x7[5], x7[10]);
  x8[10] = _mm256_subs_epi16(x7[5], x7[10]);
  x8[6] = _mm256_adds_epi16(x7[6], x7[9]);
  x8[9] = _mm256_subs_epi16(x7[6], x7[9]);
  x8[7] = _mm256_adds_epi16(x7[7], x7[8]);
  x8[8] = _mm256_subs_epi16(x7[7], x7[8]);
  x8[16] = x7[16];
  x8[17] = x7[17];
  x8[18] = x7[18];
  x8[19] = x7[19];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x7[20], x7[27], x8[20], x8[27]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x7[21], x7[26], x8[21], x8[26]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x7[22], x7[25], x8[22], x8[25]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x7[23], x7[24], x8[23], x8[24]);
  x8[28] = x7[28];
  x8[29] = x7[29];
  x8[30] = x7[30];
  x8[31] = x7[31];

  // stage 9
  output[0] = _mm256_adds_epi16(x8[0], x8[31]);
  output[31] = _mm256_subs_epi16(x8[0], x8[31]);
  output[1] = _mm256_adds_epi16(x8[1], x8[30]);
  output[30] = _mm256_subs_epi16(x8[1], x8[30]);
  output[2] = _mm256_adds_epi16(x8[2], x8[29]);
  output[29] = _mm256_subs_epi16(x8[2], x8[29]);
  output[3] = _mm256_adds_epi16(x8[3], x8[28]);
  output[28] = _mm256_subs_epi16(x8[3], x8[28]);
  output[4] = _mm256_adds_epi16(x8[4], x8[27]);
  output[27] = _mm256_subs_epi16(x8[4], x8[27]);
  output[5] = _mm256_adds_epi16(x8[5], x8[26]);
  output[26] = _mm256_subs_epi16(x8[5], x8[26]);
  output[6] = _mm256_adds_epi16(x8[6], x8[25]);
  output[25] = _mm256_subs_epi16(x8[6], x8[25]);
  output[7] = _mm256_adds_epi16(x8[7], x8[24]);
  output[24] = _mm256_subs_epi16(x8[7], x8[24]);
  output[8] = _mm256_adds_epi16(x8[8], x8[23]);
  output[23] = _mm256_subs_epi16(x8[8], x8[23]);
  output[9] = _mm256_adds_epi16(x8[9], x8[22]);
  output[22] = _mm256_subs_epi16(x8[9], x8[22]);
  output[10] = _mm256_adds_epi16(x8[10], x8[21]);
  output[21] = _mm256_subs_epi16(x8[10], x8[21]);
  output[11] = _mm256_adds_epi16(x8[11], x8[20]);
  output[20] = _mm256_subs_epi16(x8[11], x8[20]);
  output[12] = _mm256_adds_epi16(x8[12], x8[19]);
  output[19] = _mm256_subs_epi16(x8[12], x8[19]);
  output[13] = _mm256_adds_epi16(x8[13], x8[18]);
  output[18] = _mm256_subs_epi16(x8[13], x8[18]);
  output[14] = _mm256_adds_epi16(x8[14], x8[17]);
  output[17] = _mm256_subs_epi16(x8[14], x8[17]);
  output[15] = _mm256_adds_epi16(x8[15], x8[16]);
  output[16] = _mm256_subs_epi16(x8[15], x8[16]);
}

void idct64_low32_new_avx2(const __m256i *input, __m256i *output,
                           int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i __rounding = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_m04_p60 = pair_set_w16_epi16(-cospi[4], cospi[60]);
  __m256i cospi_p60_p04 = pair_set_w16_epi16(cospi[60], cospi[4]);
  __m256i cospi_m60_m04 = pair_set_w16_epi16(-cospi[60], -cospi[4]);
  __m256i cospi_m36_p28 = pair_set_w16_epi16(-cospi[36], cospi[28]);
  __m256i cospi_p28_p36 = pair_set_w16_epi16(cospi[28], cospi[36]);
  __m256i cospi_m28_m36 = pair_set_w16_epi16(-cospi[28], -cospi[36]);
  __m256i cospi_m20_p44 = pair_set_w16_epi16(-cospi[20], cospi[44]);
  __m256i cospi_p44_p20 = pair_set_w16_epi16(cospi[44], cospi[20]);
  __m256i cospi_m44_m20 = pair_set_w16_epi16(-cospi[44], -cospi[20]);
  __m256i cospi_m52_p12 = pair_set_w16_epi16(-cospi[52], cospi[12]);
  __m256i cospi_p12_p52 = pair_set_w16_epi16(cospi[12], cospi[52]);
  __m256i cospi_m12_m52 = pair_set_w16_epi16(-cospi[12], -cospi[52]);
  __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x1[64];
  x1[0] = input[0];
  x1[2] = input[16];
  x1[4] = input[8];
  x1[6] = input[24];
  x1[8] = input[4];
  x1[10] = input[20];
  x1[12] = input[12];
  x1[14] = input[28];
  x1[16] = input[2];
  x1[18] = input[18];
  x1[20] = input[10];
  x1[22] = input[26];
  x1[24] = input[6];
  x1[26] = input[22];
  x1[28] = input[14];
  x1[30] = input[30];
  x1[32] = input[1];
  x1[34] = input[17];
  x1[36] = input[9];
  x1[38] = input[25];
  x1[40] = input[5];
  x1[42] = input[21];
  x1[44] = input[13];
  x1[46] = input[29];
  x1[48] = input[3];
  x1[50] = input[19];
  x1[52] = input[11];
  x1[54] = input[27];
  x1[56] = input[7];
  x1[58] = input[23];
  x1[60] = input[15];
  x1[62] = input[31];

  // stage 2
  __m256i x2[64];
  x2[0] = x1[0];
  x2[2] = x1[2];
  x2[4] = x1[4];
  x2[6] = x1[6];
  x2[8] = x1[8];
  x2[10] = x1[10];
  x2[12] = x1[12];
  x2[14] = x1[14];
  x2[16] = x1[16];
  x2[18] = x1[18];
  x2[20] = x1[20];
  x2[22] = x1[22];
  x2[24] = x1[24];
  x2[26] = x1[26];
  x2[28] = x1[28];
  x2[30] = x1[30];

  btf_16_w16_0_avx2(cospi[63], cospi[1], x1[32], x2[32], x2[63]);
  btf_16_w16_0_avx2(-cospi[33], cospi[31], x1[62], x2[33], x2[62]);
  btf_16_w16_0_avx2(cospi[47], cospi[17], x1[34], x2[34], x2[61]);
  btf_16_w16_0_avx2(-cospi[49], cospi[15], x1[60], x2[35], x2[60]);
  btf_16_w16_0_avx2(cospi[55], cospi[9], x1[36], x2[36], x2[59]);
  btf_16_w16_0_avx2(-cospi[41], cospi[23], x1[58], x2[37], x2[58]);
  btf_16_w16_0_avx2(cospi[39], cospi[25], x1[38], x2[38], x2[57]);
  btf_16_w16_0_avx2(-cospi[57], cospi[7], x1[56], x2[39], x2[56]);
  btf_16_w16_0_avx2(cospi[59], cospi[5], x1[40], x2[40], x2[55]);
  btf_16_w16_0_avx2(-cospi[37], cospi[27], x1[54], x2[41], x2[54]);
  btf_16_w16_0_avx2(cospi[43], cospi[21], x1[42], x2[42], x2[53]);
  btf_16_w16_0_avx2(-cospi[53], cospi[11], x1[52], x2[43], x2[52]);
  btf_16_w16_0_avx2(cospi[51], cospi[13], x1[44], x2[44], x2[51]);
  btf_16_w16_0_avx2(-cospi[45], cospi[19], x1[50], x2[45], x2[50]);
  btf_16_w16_0_avx2(cospi[35], cospi[29], x1[46], x2[46], x2[49]);
  btf_16_w16_0_avx2(-cospi[61], cospi[3], x1[48], x2[47], x2[48]);

  // stage 3
  __m256i x3[64];
  x3[0] = x2[0];
  x3[2] = x2[2];
  x3[4] = x2[4];
  x3[6] = x2[6];
  x3[8] = x2[8];
  x3[10] = x2[10];
  x3[12] = x2[12];
  x3[14] = x2[14];
  btf_16_w16_0_avx2(cospi[62], cospi[2], x2[16], x3[16], x3[31]);
  btf_16_w16_0_avx2(-cospi[34], cospi[30], x2[30], x3[17], x3[30]);
  btf_16_w16_0_avx2(cospi[46], cospi[18], x2[18], x3[18], x3[29]);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x2[28], x3[19], x3[28]);
  btf_16_w16_0_avx2(cospi[54], cospi[10], x2[20], x3[20], x3[27]);
  btf_16_w16_0_avx2(-cospi[42], cospi[22], x2[26], x3[21], x3[26]);
  btf_16_w16_0_avx2(cospi[38], cospi[26], x2[22], x3[22], x3[25]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x2[24], x3[23], x3[24]);
  x3[32] = _mm256_adds_epi16(x2[32], x2[33]);
  x3[33] = _mm256_subs_epi16(x2[32], x2[33]);
  x3[34] = _mm256_subs_epi16(x2[35], x2[34]);
  x3[35] = _mm256_adds_epi16(x2[34], x2[35]);
  x3[36] = _mm256_adds_epi16(x2[36], x2[37]);
  x3[37] = _mm256_subs_epi16(x2[36], x2[37]);
  x3[38] = _mm256_subs_epi16(x2[39], x2[38]);
  x3[39] = _mm256_adds_epi16(x2[38], x2[39]);
  x3[40] = _mm256_adds_epi16(x2[40], x2[41]);
  x3[41] = _mm256_subs_epi16(x2[40], x2[41]);
  x3[42] = _mm256_subs_epi16(x2[43], x2[42]);
  x3[43] = _mm256_adds_epi16(x2[42], x2[43]);
  x3[44] = _mm256_adds_epi16(x2[44], x2[45]);
  x3[45] = _mm256_subs_epi16(x2[44], x2[45]);
  x3[46] = _mm256_subs_epi16(x2[47], x2[46]);
  x3[47] = _mm256_adds_epi16(x2[46], x2[47]);
  x3[48] = _mm256_adds_epi16(x2[48], x2[49]);
  x3[49] = _mm256_subs_epi16(x2[48], x2[49]);
  x3[50] = _mm256_subs_epi16(x2[51], x2[50]);
  x3[51] = _mm256_adds_epi16(x2[50], x2[51]);
  x3[52] = _mm256_adds_epi16(x2[52], x2[53]);
  x3[53] = _mm256_subs_epi16(x2[52], x2[53]);
  x3[54] = _mm256_subs_epi16(x2[55], x2[54]);
  x3[55] = _mm256_adds_epi16(x2[54], x2[55]);
  x3[56] = _mm256_adds_epi16(x2[56], x2[57]);
  x3[57] = _mm256_subs_epi16(x2[56], x2[57]);
  x3[58] = _mm256_subs_epi16(x2[59], x2[58]);
  x3[59] = _mm256_adds_epi16(x2[58], x2[59]);
  x3[60] = _mm256_adds_epi16(x2[60], x2[61]);
  x3[61] = _mm256_subs_epi16(x2[60], x2[61]);
  x3[62] = _mm256_subs_epi16(x2[63], x2[62]);
  x3[63] = _mm256_adds_epi16(x2[62], x2[63]);

  // stage 4
  __m256i x4[64];
  x4[0] = x3[0];
  x4[2] = x3[2];
  x4[4] = x3[4];
  x4[6] = x3[6];
  btf_16_w16_0_avx2(cospi[60], cospi[4], x3[8], x4[8], x4[15]);
  btf_16_w16_0_avx2(-cospi[36], cospi[28], x3[14], x4[9], x4[14]);
  btf_16_w16_0_avx2(cospi[44], cospi[20], x3[10], x4[10], x4[13]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x3[12], x4[11], x4[12]);
  x4[16] = _mm256_adds_epi16(x3[16], x3[17]);
  x4[17] = _mm256_subs_epi16(x3[16], x3[17]);
  x4[18] = _mm256_subs_epi16(x3[19], x3[18]);
  x4[19] = _mm256_adds_epi16(x3[18], x3[19]);
  x4[20] = _mm256_adds_epi16(x3[20], x3[21]);
  x4[21] = _mm256_subs_epi16(x3[20], x3[21]);
  x4[22] = _mm256_subs_epi16(x3[23], x3[22]);
  x4[23] = _mm256_adds_epi16(x3[22], x3[23]);
  x4[24] = _mm256_adds_epi16(x3[24], x3[25]);
  x4[25] = _mm256_subs_epi16(x3[24], x3[25]);
  x4[26] = _mm256_subs_epi16(x3[27], x3[26]);
  x4[27] = _mm256_adds_epi16(x3[26], x3[27]);
  x4[28] = _mm256_adds_epi16(x3[28], x3[29]);
  x4[29] = _mm256_subs_epi16(x3[28], x3[29]);
  x4[30] = _mm256_subs_epi16(x3[31], x3[30]);
  x4[31] = _mm256_adds_epi16(x3[30], x3[31]);
  x4[32] = x3[32];
  btf_16_w16_avx2(cospi_m04_p60, cospi_p60_p04, x3[33], x3[62], x4[33], x4[62]);
  btf_16_w16_avx2(cospi_m60_m04, cospi_m04_p60, x3[34], x3[61], x4[34], x4[61]);
  x4[35] = x3[35];
  x4[36] = x3[36];
  btf_16_w16_avx2(cospi_m36_p28, cospi_p28_p36, x3[37], x3[58], x4[37], x4[58]);
  btf_16_w16_avx2(cospi_m28_m36, cospi_m36_p28, x3[38], x3[57], x4[38], x4[57]);
  x4[39] = x3[39];
  x4[40] = x3[40];
  btf_16_w16_avx2(cospi_m20_p44, cospi_p44_p20, x3[41], x3[54], x4[41], x4[54]);
  btf_16_w16_avx2(cospi_m44_m20, cospi_m20_p44, x3[42], x3[53], x4[42], x4[53]);
  x4[43] = x3[43];
  x4[44] = x3[44];
  btf_16_w16_avx2(cospi_m52_p12, cospi_p12_p52, x3[45], x3[50], x4[45], x4[50]);
  btf_16_w16_avx2(cospi_m12_m52, cospi_m52_p12, x3[46], x3[49], x4[46], x4[49]);
  x4[47] = x3[47];
  x4[48] = x3[48];
  x4[51] = x3[51];
  x4[52] = x3[52];
  x4[55] = x3[55];
  x4[56] = x3[56];
  x4[59] = x3[59];
  x4[60] = x3[60];
  x4[63] = x3[63];

  // stage 5
  __m256i x5[64];
  x5[0] = x4[0];
  x5[2] = x4[2];
  btf_16_w16_0_avx2(cospi[56], cospi[8], x4[4], x5[4], x5[7]);
  btf_16_w16_0_avx2(-cospi[40], cospi[24], x4[6], x5[5], x5[6]);
  x5[8] = _mm256_adds_epi16(x4[8], x4[9]);
  x5[9] = _mm256_subs_epi16(x4[8], x4[9]);
  x5[10] = _mm256_subs_epi16(x4[11], x4[10]);
  x5[11] = _mm256_adds_epi16(x4[10], x4[11]);
  x5[12] = _mm256_adds_epi16(x4[12], x4[13]);
  x5[13] = _mm256_subs_epi16(x4[12], x4[13]);
  x5[14] = _mm256_subs_epi16(x4[15], x4[14]);
  x5[15] = _mm256_adds_epi16(x4[14], x4[15]);
  x5[16] = x4[16];
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, x4[17], x4[30], x5[17], x5[30]);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, x4[18], x4[29], x5[18], x5[29]);
  x5[19] = x4[19];
  x5[20] = x4[20];
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, x4[21], x4[26], x5[21], x5[26]);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, x4[22], x4[25], x5[22], x5[25]);
  x5[23] = x4[23];
  x5[24] = x4[24];
  x5[27] = x4[27];
  x5[28] = x4[28];
  x5[31] = x4[31];
  x5[32] = _mm256_adds_epi16(x4[32], x4[35]);
  x5[35] = _mm256_subs_epi16(x4[32], x4[35]);
  x5[33] = _mm256_adds_epi16(x4[33], x4[34]);
  x5[34] = _mm256_subs_epi16(x4[33], x4[34]);
  x5[36] = _mm256_subs_epi16(x4[39], x4[36]);
  x5[39] = _mm256_adds_epi16(x4[36], x4[39]);
  x5[37] = _mm256_subs_epi16(x4[38], x4[37]);
  x5[38] = _mm256_adds_epi16(x4[37], x4[38]);
  x5[40] = _mm256_adds_epi16(x4[40], x4[43]);
  x5[43] = _mm256_subs_epi16(x4[40], x4[43]);
  x5[41] = _mm256_adds_epi16(x4[41], x4[42]);
  x5[42] = _mm256_subs_epi16(x4[41], x4[42]);
  x5[44] = _mm256_subs_epi16(x4[47], x4[44]);
  x5[47] = _mm256_adds_epi16(x4[44], x4[47]);
  x5[45] = _mm256_subs_epi16(x4[46], x4[45]);
  x5[46] = _mm256_adds_epi16(x4[45], x4[46]);
  x5[48] = _mm256_adds_epi16(x4[48], x4[51]);
  x5[51] = _mm256_subs_epi16(x4[48], x4[51]);
  x5[49] = _mm256_adds_epi16(x4[49], x4[50]);
  x5[50] = _mm256_subs_epi16(x4[49], x4[50]);
  x5[52] = _mm256_subs_epi16(x4[55], x4[52]);
  x5[55] = _mm256_adds_epi16(x4[52], x4[55]);
  x5[53] = _mm256_subs_epi16(x4[54], x4[53]);
  x5[54] = _mm256_adds_epi16(x4[53], x4[54]);
  x5[56] = _mm256_adds_epi16(x4[56], x4[59]);
  x5[59] = _mm256_subs_epi16(x4[56], x4[59]);
  x5[57] = _mm256_adds_epi16(x4[57], x4[58]);
  x5[58] = _mm256_subs_epi16(x4[57], x4[58]);
  x5[60] = _mm256_subs_epi16(x4[63], x4[60]);
  x5[63] = _mm256_adds_epi16(x4[60], x4[63]);
  x5[61] = _mm256_subs_epi16(x4[62], x4[61]);
  x5[62] = _mm256_adds_epi16(x4[61], x4[62]);

  // stage 6
  __m256i x6[64];
  btf_16_w16_0_avx2(cospi[32], cospi[32], x5[0], x6[0], x6[1]);
  btf_16_w16_0_avx2(cospi[48], cospi[16], x5[2], x6[2], x6[3]);
  x6[4] = _mm256_adds_epi16(x5[4], x5[5]);
  x6[5] = _mm256_subs_epi16(x5[4], x5[5]);
  x6[6] = _mm256_subs_epi16(x5[7], x5[6]);
  x6[7] = _mm256_adds_epi16(x5[6], x5[7]);
  x6[8] = x5[8];
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x5[9], x5[14], x6[9], x6[14]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x5[10], x5[13], x6[10], x6[13]);
  x6[11] = x5[11];
  x6[12] = x5[12];
  x6[15] = x5[15];
  x6[16] = _mm256_adds_epi16(x5[16], x5[19]);
  x6[19] = _mm256_subs_epi16(x5[16], x5[19]);
  x6[17] = _mm256_adds_epi16(x5[17], x5[18]);
  x6[18] = _mm256_subs_epi16(x5[17], x5[18]);
  x6[20] = _mm256_subs_epi16(x5[23], x5[20]);
  x6[23] = _mm256_adds_epi16(x5[20], x5[23]);
  x6[21] = _mm256_subs_epi16(x5[22], x5[21]);
  x6[22] = _mm256_adds_epi16(x5[21], x5[22]);
  x6[24] = _mm256_adds_epi16(x5[24], x5[27]);
  x6[27] = _mm256_subs_epi16(x5[24], x5[27]);
  x6[25] = _mm256_adds_epi16(x5[25], x5[26]);
  x6[26] = _mm256_subs_epi16(x5[25], x5[26]);
  x6[28] = _mm256_subs_epi16(x5[31], x5[28]);
  x6[31] = _mm256_adds_epi16(x5[28], x5[31]);
  x6[29] = _mm256_subs_epi16(x5[30], x5[29]);
  x6[30] = _mm256_adds_epi16(x5[29], x5[30]);
  x6[32] = x5[32];
  x6[33] = x5[33];
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, x5[34], x5[61], x6[34], x6[61]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, x5[35], x5[60], x6[35], x6[60]);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, x5[36], x5[59], x6[36], x6[59]);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, x5[37], x5[58], x6[37], x6[58]);
  x6[38] = x5[38];
  x6[39] = x5[39];
  x6[40] = x5[40];
  x6[41] = x5[41];
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, x5[42], x5[53], x6[42], x6[53]);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, x5[43], x5[52], x6[43], x6[52]);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, x5[44], x5[51], x6[44], x6[51]);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, x5[45], x5[50], x6[45], x6[50]);
  x6[46] = x5[46];
  x6[47] = x5[47];
  x6[48] = x5[48];
  x6[49] = x5[49];
  x6[54] = x5[54];
  x6[55] = x5[55];
  x6[56] = x5[56];
  x6[57] = x5[57];
  x6[62] = x5[62];
  x6[63] = x5[63];

  // stage 7
  __m256i x7[64];
  x7[0] = _mm256_adds_epi16(x6[0], x6[3]);
  x7[3] = _mm256_subs_epi16(x6[0], x6[3]);
  x7[1] = _mm256_adds_epi16(x6[1], x6[2]);
  x7[2] = _mm256_subs_epi16(x6[1], x6[2]);
  x7[4] = x6[4];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x6[5], x6[6], x7[5], x7[6]);
  x7[7] = x6[7];
  x7[8] = _mm256_adds_epi16(x6[8], x6[11]);
  x7[11] = _mm256_subs_epi16(x6[8], x6[11]);
  x7[9] = _mm256_adds_epi16(x6[9], x6[10]);
  x7[10] = _mm256_subs_epi16(x6[9], x6[10]);
  x7[12] = _mm256_subs_epi16(x6[15], x6[12]);
  x7[15] = _mm256_adds_epi16(x6[12], x6[15]);
  x7[13] = _mm256_subs_epi16(x6[14], x6[13]);
  x7[14] = _mm256_adds_epi16(x6[13], x6[14]);
  x7[16] = x6[16];
  x7[17] = x6[17];
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x6[18], x6[29], x7[18], x7[29]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x6[19], x6[28], x7[19], x7[28]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x6[20], x6[27], x7[20], x7[27]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x6[21], x6[26], x7[21], x7[26]);
  x7[22] = x6[22];
  x7[23] = x6[23];
  x7[24] = x6[24];
  x7[25] = x6[25];
  x7[30] = x6[30];
  x7[31] = x6[31];
  x7[32] = _mm256_adds_epi16(x6[32], x6[39]);
  x7[39] = _mm256_subs_epi16(x6[32], x6[39]);
  x7[33] = _mm256_adds_epi16(x6[33], x6[38]);
  x7[38] = _mm256_subs_epi16(x6[33], x6[38]);
  x7[34] = _mm256_adds_epi16(x6[34], x6[37]);
  x7[37] = _mm256_subs_epi16(x6[34], x6[37]);
  x7[35] = _mm256_adds_epi16(x6[35], x6[36]);
  x7[36] = _mm256_subs_epi16(x6[35], x6[36]);
  x7[40] = _mm256_subs_epi16(x6[47], x6[40]);
  x7[47] = _mm256_adds_epi16(x6[40], x6[47]);
  x7[41] = _mm256_subs_epi16(x6[46], x6[41]);
  x7[46] = _mm256_adds_epi16(x6[41], x6[46]);
  x7[42] = _mm256_subs_epi16(x6[45], x6[42]);
  x7[45] = _mm256_adds_epi16(x6[42], x6[45]);
  x7[43] = _mm256_subs_epi16(x6[44], x6[43]);
  x7[44] = _mm256_adds_epi16(x6[43], x6[44]);
  x7[48] = _mm256_adds_epi16(x6[48], x6[55]);
  x7[55] = _mm256_subs_epi16(x6[48], x6[55]);
  x7[49] = _mm256_adds_epi16(x6[49], x6[54]);
  x7[54] = _mm256_subs_epi16(x6[49], x6[54]);
  x7[50] = _mm256_adds_epi16(x6[50], x6[53]);
  x7[53] = _mm256_subs_epi16(x6[50], x6[53]);
  x7[51] = _mm256_adds_epi16(x6[51], x6[52]);
  x7[52] = _mm256_subs_epi16(x6[51], x6[52]);
  x7[56] = _mm256_subs_epi16(x6[63], x6[56]);
  x7[63] = _mm256_adds_epi16(x6[56], x6[63]);
  x7[57] = _mm256_subs_epi16(x6[62], x6[57]);
  x7[62] = _mm256_adds_epi16(x6[57], x6[62]);
  x7[58] = _mm256_subs_epi16(x6[61], x6[58]);
  x7[61] = _mm256_adds_epi16(x6[58], x6[61]);
  x7[59] = _mm256_subs_epi16(x6[60], x6[59]);
  x7[60] = _mm256_adds_epi16(x6[59], x6[60]);

  // stage 8
  __m256i x8[64];
  x8[0] = _mm256_adds_epi16(x7[0], x7[7]);
  x8[7] = _mm256_subs_epi16(x7[0], x7[7]);
  x8[1] = _mm256_adds_epi16(x7[1], x7[6]);
  x8[6] = _mm256_subs_epi16(x7[1], x7[6]);
  x8[2] = _mm256_adds_epi16(x7[2], x7[5]);
  x8[5] = _mm256_subs_epi16(x7[2], x7[5]);
  x8[3] = _mm256_adds_epi16(x7[3], x7[4]);
  x8[4] = _mm256_subs_epi16(x7[3], x7[4]);
  x8[8] = x7[8];
  x8[9] = x7[9];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x7[10], x7[13], x8[10], x8[13]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x7[11], x7[12], x8[11], x8[12]);
  x8[14] = x7[14];
  x8[15] = x7[15];
  x8[16] = _mm256_adds_epi16(x7[16], x7[23]);
  x8[23] = _mm256_subs_epi16(x7[16], x7[23]);
  x8[17] = _mm256_adds_epi16(x7[17], x7[22]);
  x8[22] = _mm256_subs_epi16(x7[17], x7[22]);
  x8[18] = _mm256_adds_epi16(x7[18], x7[21]);
  x8[21] = _mm256_subs_epi16(x7[18], x7[21]);
  x8[19] = _mm256_adds_epi16(x7[19], x7[20]);
  x8[20] = _mm256_subs_epi16(x7[19], x7[20]);
  x8[24] = _mm256_subs_epi16(x7[31], x7[24]);
  x8[31] = _mm256_adds_epi16(x7[24], x7[31]);
  x8[25] = _mm256_subs_epi16(x7[30], x7[25]);
  x8[30] = _mm256_adds_epi16(x7[25], x7[30]);
  x8[26] = _mm256_subs_epi16(x7[29], x7[26]);
  x8[29] = _mm256_adds_epi16(x7[26], x7[29]);
  x8[27] = _mm256_subs_epi16(x7[28], x7[27]);
  x8[28] = _mm256_adds_epi16(x7[27], x7[28]);
  x8[32] = x7[32];
  x8[33] = x7[33];
  x8[34] = x7[34];
  x8[35] = x7[35];
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x7[36], x7[59], x8[36], x8[59]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x7[37], x7[58], x8[37], x8[58]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x7[38], x7[57], x8[38], x8[57]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, x7[39], x7[56], x8[39], x8[56]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x7[40], x7[55], x8[40], x8[55]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x7[41], x7[54], x8[41], x8[54]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x7[42], x7[53], x8[42], x8[53]);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, x7[43], x7[52], x8[43], x8[52]);
  x8[44] = x7[44];
  x8[45] = x7[45];
  x8[46] = x7[46];
  x8[47] = x7[47];
  x8[48] = x7[48];
  x8[49] = x7[49];
  x8[50] = x7[50];
  x8[51] = x7[51];
  x8[60] = x7[60];
  x8[61] = x7[61];
  x8[62] = x7[62];
  x8[63] = x7[63];

  // stage 9
  __m256i x9[64];
  x9[0] = _mm256_adds_epi16(x8[0], x8[15]);
  x9[15] = _mm256_subs_epi16(x8[0], x8[15]);
  x9[1] = _mm256_adds_epi16(x8[1], x8[14]);
  x9[14] = _mm256_subs_epi16(x8[1], x8[14]);
  x9[2] = _mm256_adds_epi16(x8[2], x8[13]);
  x9[13] = _mm256_subs_epi16(x8[2], x8[13]);
  x9[3] = _mm256_adds_epi16(x8[3], x8[12]);
  x9[12] = _mm256_subs_epi16(x8[3], x8[12]);
  x9[4] = _mm256_adds_epi16(x8[4], x8[11]);
  x9[11] = _mm256_subs_epi16(x8[4], x8[11]);
  x9[5] = _mm256_adds_epi16(x8[5], x8[10]);
  x9[10] = _mm256_subs_epi16(x8[5], x8[10]);
  x9[6] = _mm256_adds_epi16(x8[6], x8[9]);
  x9[9] = _mm256_subs_epi16(x8[6], x8[9]);
  x9[7] = _mm256_adds_epi16(x8[7], x8[8]);
  x9[8] = _mm256_subs_epi16(x8[7], x8[8]);
  x9[16] = x8[16];
  x9[17] = x8[17];
  x9[18] = x8[18];
  x9[19] = x8[19];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x8[20], x8[27], x9[20], x9[27]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x8[21], x8[26], x9[21], x9[26]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x8[22], x8[25], x9[22], x9[25]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x8[23], x8[24], x9[23], x9[24]);
  x9[28] = x8[28];
  x9[29] = x8[29];
  x9[30] = x8[30];
  x9[31] = x8[31];
  x9[32] = _mm256_adds_epi16(x8[32], x8[47]);
  x9[47] = _mm256_subs_epi16(x8[32], x8[47]);
  x9[33] = _mm256_adds_epi16(x8[33], x8[46]);
  x9[46] = _mm256_subs_epi16(x8[33], x8[46]);
  x9[34] = _mm256_adds_epi16(x8[34], x8[45]);
  x9[45] = _mm256_subs_epi16(x8[34], x8[45]);
  x9[35] = _mm256_adds_epi16(x8[35], x8[44]);
  x9[44] = _mm256_subs_epi16(x8[35], x8[44]);
  x9[36] = _mm256_adds_epi16(x8[36], x8[43]);
  x9[43] = _mm256_subs_epi16(x8[36], x8[43]);
  x9[37] = _mm256_adds_epi16(x8[37], x8[42]);
  x9[42] = _mm256_subs_epi16(x8[37], x8[42]);
  x9[38] = _mm256_adds_epi16(x8[38], x8[41]);
  x9[41] = _mm256_subs_epi16(x8[38], x8[41]);
  x9[39] = _mm256_adds_epi16(x8[39], x8[40]);
  x9[40] = _mm256_subs_epi16(x8[39], x8[40]);
  x9[48] = _mm256_subs_epi16(x8[63], x8[48]);
  x9[63] = _mm256_adds_epi16(x8[48], x8[63]);
  x9[49] = _mm256_subs_epi16(x8[62], x8[49]);
  x9[62] = _mm256_adds_epi16(x8[49], x8[62]);
  x9[50] = _mm256_subs_epi16(x8[61], x8[50]);
  x9[61] = _mm256_adds_epi16(x8[50], x8[61]);
  x9[51] = _mm256_subs_epi16(x8[60], x8[51]);
  x9[60] = _mm256_adds_epi16(x8[51], x8[60]);
  x9[52] = _mm256_subs_epi16(x8[59], x8[52]);
  x9[59] = _mm256_adds_epi16(x8[52], x8[59]);
  x9[53] = _mm256_subs_epi16(x8[58], x8[53]);
  x9[58] = _mm256_adds_epi16(x8[53], x8[58]);
  x9[54] = _mm256_subs_epi16(x8[57], x8[54]);
  x9[57] = _mm256_adds_epi16(x8[54], x8[57]);
  x9[55] = _mm256_subs_epi16(x8[56], x8[55]);
  x9[56] = _mm256_adds_epi16(x8[55], x8[56]);

  // stage 10
  __m256i x10[64];
  x10[0] = _mm256_adds_epi16(x9[0], x9[31]);
  x10[31] = _mm256_subs_epi16(x9[0], x9[31]);
  x10[1] = _mm256_adds_epi16(x9[1], x9[30]);
  x10[30] = _mm256_subs_epi16(x9[1], x9[30]);
  x10[2] = _mm256_adds_epi16(x9[2], x9[29]);
  x10[29] = _mm256_subs_epi16(x9[2], x9[29]);
  x10[3] = _mm256_adds_epi16(x9[3], x9[28]);
  x10[28] = _mm256_subs_epi16(x9[3], x9[28]);
  x10[4] = _mm256_adds_epi16(x9[4], x9[27]);
  x10[27] = _mm256_subs_epi16(x9[4], x9[27]);
  x10[5] = _mm256_adds_epi16(x9[5], x9[26]);
  x10[26] = _mm256_subs_epi16(x9[5], x9[26]);
  x10[6] = _mm256_adds_epi16(x9[6], x9[25]);
  x10[25] = _mm256_subs_epi16(x9[6], x9[25]);
  x10[7] = _mm256_adds_epi16(x9[7], x9[24]);
  x10[24] = _mm256_subs_epi16(x9[7], x9[24]);
  x10[8] = _mm256_adds_epi16(x9[8], x9[23]);
  x10[23] = _mm256_subs_epi16(x9[8], x9[23]);
  x10[9] = _mm256_adds_epi16(x9[9], x9[22]);
  x10[22] = _mm256_subs_epi16(x9[9], x9[22]);
  x10[10] = _mm256_adds_epi16(x9[10], x9[21]);
  x10[21] = _mm256_subs_epi16(x9[10], x9[21]);
  x10[11] = _mm256_adds_epi16(x9[11], x9[20]);
  x10[20] = _mm256_subs_epi16(x9[11], x9[20]);
  x10[12] = _mm256_adds_epi16(x9[12], x9[19]);
  x10[19] = _mm256_subs_epi16(x9[12], x9[19]);
  x10[13] = _mm256_adds_epi16(x9[13], x9[18]);
  x10[18] = _mm256_subs_epi16(x9[13], x9[18]);
  x10[14] = _mm256_adds_epi16(x9[14], x9[17]);
  x10[17] = _mm256_subs_epi16(x9[14], x9[17]);
  x10[15] = _mm256_adds_epi16(x9[15], x9[16]);
  x10[16] = _mm256_subs_epi16(x9[15], x9[16]);
  x10[32] = x9[32];
  x10[33] = x9[33];
  x10[34] = x9[34];
  x10[35] = x9[35];
  x10[36] = x9[36];
  x10[37] = x9[37];
  x10[38] = x9[38];
  x10[39] = x9[39];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x9[40], x9[55], x10[40],
                  x10[55]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x9[41], x9[54], x10[41],
                  x10[54]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x9[42], x9[53], x10[42],
                  x10[53]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x9[43], x9[52], x10[43],
                  x10[52]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x9[44], x9[51], x10[44],
                  x10[51]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x9[45], x9[50], x10[45],
                  x10[50]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x9[46], x9[49], x10[46],
                  x10[49]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, x9[47], x9[48], x10[47],
                  x10[48]);
  x10[56] = x9[56];
  x10[57] = x9[57];
  x10[58] = x9[58];
  x10[59] = x9[59];
  x10[60] = x9[60];
  x10[61] = x9[61];
  x10[62] = x9[62];
  x10[63] = x9[63];

  // stage 11
  output[0] = _mm256_adds_epi16(x10[0], x10[63]);
  output[63] = _mm256_subs_epi16(x10[0], x10[63]);
  output[1] = _mm256_adds_epi16(x10[1], x10[62]);
  output[62] = _mm256_subs_epi16(x10[1], x10[62]);
  output[2] = _mm256_adds_epi16(x10[2], x10[61]);
  output[61] = _mm256_subs_epi16(x10[2], x10[61]);
  output[3] = _mm256_adds_epi16(x10[3], x10[60]);
  output[60] = _mm256_subs_epi16(x10[3], x10[60]);
  output[4] = _mm256_adds_epi16(x10[4], x10[59]);
  output[59] = _mm256_subs_epi16(x10[4], x10[59]);
  output[5] = _mm256_adds_epi16(x10[5], x10[58]);
  output[58] = _mm256_subs_epi16(x10[5], x10[58]);
  output[6] = _mm256_adds_epi16(x10[6], x10[57]);
  output[57] = _mm256_subs_epi16(x10[6], x10[57]);
  output[7] = _mm256_adds_epi16(x10[7], x10[56]);
  output[56] = _mm256_subs_epi16(x10[7], x10[56]);
  output[8] = _mm256_adds_epi16(x10[8], x10[55]);
  output[55] = _mm256_subs_epi16(x10[8], x10[55]);
  output[9] = _mm256_adds_epi16(x10[9], x10[54]);
  output[54] = _mm256_subs_epi16(x10[9], x10[54]);
  output[10] = _mm256_adds_epi16(x10[10], x10[53]);
  output[53] = _mm256_subs_epi16(x10[10], x10[53]);
  output[11] = _mm256_adds_epi16(x10[11], x10[52]);
  output[52] = _mm256_subs_epi16(x10[11], x10[52]);
  output[12] = _mm256_adds_epi16(x10[12], x10[51]);
  output[51] = _mm256_subs_epi16(x10[12], x10[51]);
  output[13] = _mm256_adds_epi16(x10[13], x10[50]);
  output[50] = _mm256_subs_epi16(x10[13], x10[50]);
  output[14] = _mm256_adds_epi16(x10[14], x10[49]);
  output[49] = _mm256_subs_epi16(x10[14], x10[49]);
  output[15] = _mm256_adds_epi16(x10[15], x10[48]);
  output[48] = _mm256_subs_epi16(x10[15], x10[48]);
  output[16] = _mm256_adds_epi16(x10[16], x10[47]);
  output[47] = _mm256_subs_epi16(x10[16], x10[47]);
  output[17] = _mm256_adds_epi16(x10[17], x10[46]);
  output[46] = _mm256_subs_epi16(x10[17], x10[46]);
  output[18] = _mm256_adds_epi16(x10[18], x10[45]);
  output[45] = _mm256_subs_epi16(x10[18], x10[45]);
  output[19] = _mm256_adds_epi16(x10[19], x10[44]);
  output[44] = _mm256_subs_epi16(x10[19], x10[44]);
  output[20] = _mm256_adds_epi16(x10[20], x10[43]);
  output[43] = _mm256_subs_epi16(x10[20], x10[43]);
  output[21] = _mm256_adds_epi16(x10[21], x10[42]);
  output[42] = _mm256_subs_epi16(x10[21], x10[42]);
  output[22] = _mm256_adds_epi16(x10[22], x10[41]);
  output[41] = _mm256_subs_epi16(x10[22], x10[41]);
  output[23] = _mm256_adds_epi16(x10[23], x10[40]);
  output[40] = _mm256_subs_epi16(x10[23], x10[40]);
  output[24] = _mm256_adds_epi16(x10[24], x10[39]);
  output[39] = _mm256_subs_epi16(x10[24], x10[39]);
  output[25] = _mm256_adds_epi16(x10[25], x10[38]);
  output[38] = _mm256_subs_epi16(x10[25], x10[38]);
  output[26] = _mm256_adds_epi16(x10[26], x10[37]);
  output[37] = _mm256_subs_epi16(x10[26], x10[37]);
  output[27] = _mm256_adds_epi16(x10[27], x10[36]);
  output[36] = _mm256_subs_epi16(x10[27], x10[36]);
  output[28] = _mm256_adds_epi16(x10[28], x10[35]);
  output[35] = _mm256_subs_epi16(x10[28], x10[35]);
  output[29] = _mm256_adds_epi16(x10[29], x10[34]);
  output[34] = _mm256_subs_epi16(x10[29], x10[34]);
  output[30] = _mm256_adds_epi16(x10[30], x10[33]);
  output[33] = _mm256_subs_epi16(x10[30], x10[33]);
  output[31] = _mm256_adds_epi16(x10[31], x10[32]);
  output[32] = _mm256_subs_epi16(x10[31], x10[32]);
}

static INLINE void iidentity16_row_16xn_avx2(__m256i *out, const int32_t *input,
                                             int stride, int shift,
                                             int height) {
  const int32_t *input_row = input;
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  const int16_t scale_fractional = 2 * NewSqrt2 - (2 << NewSqrt2Bits);
  const __m256i scale =
      _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  for (int h = 0; h < height; ++h) {
    __m256i src = load_32bit_to_16bit_w16_avx2(input_row);
    input_row += stride;
    __m256i x = _mm256_mulhrs_epi16(src, scale);
    __m256i srcx2 = _mm256_adds_epi16(src, src);
    x = _mm256_adds_epi16(x, srcx2);
    out[h] = _mm256_mulhrs_epi16(x, mshift);
  }
}

static INLINE void iidentity16_row_rect_16xn_avx2(__m256i *out,
                                                  const int32_t *input,
                                                  int stride, int shift,
                                                  int height) {
  const int32_t *input_row = input;
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  const int16_t scale_fractional = 2 * NewSqrt2 - (2 << NewSqrt2Bits);
  const __m256i scale =
      _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  const __m256i rect_scale =
      _mm256_set1_epi16(NewInvSqrt2 << (15 - NewSqrt2Bits));
  for (int h = 0; h < height; ++h) {
    __m256i src = load_32bit_to_16bit_w16_avx2(input_row);
    input_row += stride;
    src = _mm256_mulhrs_epi16(src, rect_scale);
    __m256i x = _mm256_mulhrs_epi16(src, scale);
    __m256i srcx2 = _mm256_adds_epi16(src, src);
    x = _mm256_adds_epi16(x, srcx2);
    out[h] = _mm256_mulhrs_epi16(x, mshift);
  }
}

static INLINE void iidentity16_col_16xn_avx2(uint8_t *output, int stride,
                                             __m256i *buf, int shift,
                                             int height) {
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  const int16_t scale_fractional = 2 * NewSqrt2 - (2 << NewSqrt2Bits);
  const __m256i scale =
      _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  for (int h = 0; h < height; ++h) {
    __m256i x = _mm256_mulhrs_epi16(buf[h], scale);
    __m256i srcx2 = _mm256_adds_epi16(buf[h], buf[h]);
    x = _mm256_adds_epi16(x, srcx2);
    x = _mm256_mulhrs_epi16(x, mshift);
    write_recon_w16_avx2(x, output);
    output += stride;
  }
}

static INLINE void iidentity32_row_16xn_avx2(__m256i *out, const int32_t *input,
                                             int stride, int shift,
                                             int height) {
  const int32_t *input_row = input;
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  for (int h = 0; h < height; ++h) {
    __m256i x = load_32bit_to_16bit_w16_avx2(input_row);
    input_row += stride;
    x = _mm256_adds_epi16(x, x);
    x = _mm256_adds_epi16(x, x);
    out[h] = _mm256_mulhrs_epi16(x, mshift);
  }
}

static INLINE void iidentity32_row_rect_16xn_avx2(__m256i *out,
                                                  const int32_t *input,
                                                  int stride, int shift,
                                                  int height) {
  const int32_t *input_row = input;
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  const __m256i rect_scale = _mm256_set1_epi16(NewInvSqrt2 * 8);
  for (int h = 0; h < height; ++h) {
    __m256i x = load_32bit_to_16bit_w16_avx2(input_row);
    input_row += stride;
    x = _mm256_mulhrs_epi16(x, rect_scale);
    x = _mm256_adds_epi16(x, x);
    x = _mm256_adds_epi16(x, x);
    out[h] = _mm256_mulhrs_epi16(x, mshift);
  }
}

static INLINE void iidentity32_col_16xn_avx2(uint8_t *output, int stride,
                                             __m256i *buf, int shift,
                                             int height) {
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  for (int h = 0; h < height; ++h) {
    __m256i x = _mm256_adds_epi16(buf[h], buf[h]);
    x = _mm256_adds_epi16(x, x);
    x = _mm256_mulhrs_epi16(x, mshift);
    write_recon_w16_avx2(x, output);
    output += stride;
  }
}

static INLINE void iidentity64_row_16xn_avx2(__m256i *out, const int32_t *input,
                                             int stride, int shift,
                                             int height) {
  const int32_t *input_row = input;
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  const int16_t scale_fractional = 4 * NewSqrt2 - (5 << NewSqrt2Bits);
  const __m256i scale =
      _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  for (int h = 0; h < height; ++h) {
    __m256i src = load_32bit_to_16bit_w16_avx2(input_row);
    input_row += stride;
    __m256i x = _mm256_mulhrs_epi16(src, scale);
    __m256i srcx5 = _mm256_adds_epi16(src, src);
    srcx5 = _mm256_adds_epi16(srcx5, srcx5);
    srcx5 = _mm256_adds_epi16(srcx5, src);
    x = _mm256_adds_epi16(x, srcx5);
    out[h] = _mm256_mulhrs_epi16(x, mshift);
  }
}

static INLINE void iidentity64_row_rect_16xn_avx2(__m256i *out,
                                                  const int32_t *input,
                                                  int stride, int shift,
                                                  int height) {
  const int32_t *input_row = input;
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  const int16_t scale_fractional = 4 * NewSqrt2 - (5 << NewSqrt2Bits);
  const __m256i scale =
      _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  const __m256i rect_scale =
      _mm256_set1_epi16(NewInvSqrt2 << (15 - NewSqrt2Bits));
  for (int h = 0; h < height; ++h) {
    __m256i src = load_32bit_to_16bit_w16_avx2(input_row);
    input_row += stride;
    src = _mm256_mulhrs_epi16(src, rect_scale);
    __m256i x = _mm256_mulhrs_epi16(src, scale);
    __m256i srcx5 = _mm256_adds_epi16(src, src);
    srcx5 = _mm256_adds_epi16(srcx5, srcx5);
    srcx5 = _mm256_adds_epi16(srcx5, src);
    x = _mm256_adds_epi16(x, srcx5);
    out[h] = _mm256_mulhrs_epi16(x, mshift);
  }
}

static INLINE void iidentity64_col_16xn_avx2(uint8_t *output, int stride,
                                             __m256i *buf, int shift,
                                             int height) {
  const __m256i mshift = _mm256_set1_epi16(1 << (15 + shift));
  const int16_t scale_fractional = 4 * NewSqrt2 - (5 << NewSqrt2Bits);
  const __m256i scale =
      _mm256_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  for (int h = 0; h < height; ++h) {
    __m256i x = _mm256_mulhrs_epi16(buf[h], scale);
    __m256i srcx5 = _mm256_adds_epi16(buf[h], buf[h]);
    srcx5 = _mm256_adds_epi16(srcx5, srcx5);
    srcx5 = _mm256_adds_epi16(srcx5, buf[h]);
    x = _mm256_adds_epi16(x, srcx5);
    x = _mm256_mulhrs_epi16(x, mshift);
    write_recon_w16_avx2(x, output);
    output += stride;
  }
}

static INLINE void identity_row_16xn_avx2(__m256i *out, const int32_t *input,
                                          int stride, int shift, int height,
                                          int txw_idx, int rect_type) {
  if (rect_type != 1 && rect_type != -1) {
    switch (txw_idx) {
      case 2:
        iidentity16_row_16xn_avx2(out, input, stride, shift, height);
        break;
      case 3:
        iidentity32_row_16xn_avx2(out, input, stride, shift, height);
        break;
      case 4:
        iidentity64_row_16xn_avx2(out, input, stride, shift, height);
        break;
      default: break;
    }
  } else {
    switch (txw_idx) {
      case 2:
        iidentity16_row_rect_16xn_avx2(out, input, stride, shift, height);
        break;
      case 3:
        iidentity32_row_rect_16xn_avx2(out, input, stride, shift, height);
        break;
      case 4:
        iidentity64_row_rect_16xn_avx2(out, input, stride, shift, height);
        break;
      default: break;
    }
  }
}

static INLINE void identity_col_16xn_avx2(uint8_t *output, int stride,
                                          __m256i *buf, int shift, int height,
                                          int txh_idx) {
  switch (txh_idx) {
    case 2:
      iidentity16_col_16xn_avx2(output, stride, buf, shift, height);
      break;
    case 3:
      iidentity32_col_16xn_avx2(output, stride, buf, shift, height);
      break;
    case 4:
      iidentity64_col_16xn_avx2(output, stride, buf, shift, height);
      break;
    default: break;
  }
}
