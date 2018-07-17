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

#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "av1/common/av1_inv_txfm1d.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/enums.h"
#include "av1/common/idct.h"
#include "av1/common/arm/av1_inv_txfm_neon.h"
#include "av1/common/arm/transpose_neon.h"

static INLINE TxSetType find_TxSetType(TX_SIZE tx_size) {
  const TX_SIZE tx_size_sqr_up = txsize_sqr_up_map[tx_size];
  TxSetType tx_set_type;
  if (tx_size_sqr_up > TX_32X32) {
    tx_set_type = EXT_TX_SET_DCTONLY;
  } else if (tx_size_sqr_up == TX_32X32) {
    tx_set_type = EXT_TX_SET_DCT_IDTX;
  } else {
    tx_set_type = EXT_TX_SET_ALL16;
  }
  return tx_set_type;
}

// 1D itx types
typedef enum ATTRIBUTE_PACKED {
  IDCT_1D,
  IADST_1D,
  IFLIPADST_1D = IADST_1D,
  IIDENTITY_1D,
  ITX_TYPES_1D,
} ITX_TYPE_1D;

static const ITX_TYPE_1D vitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IADST_1D,     IDCT_1D,      IADST_1D,
  IFLIPADST_1D, IDCT_1D,      IFLIPADST_1D, IADST_1D,
  IFLIPADST_1D, IIDENTITY_1D, IDCT_1D,      IIDENTITY_1D,
  IADST_1D,     IIDENTITY_1D, IFLIPADST_1D, IIDENTITY_1D,
};

static const ITX_TYPE_1D hitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IDCT_1D,      IADST_1D,     IADST_1D,
  IDCT_1D,      IFLIPADST_1D, IFLIPADST_1D, IFLIPADST_1D,
  IADST_1D,     IIDENTITY_1D, IIDENTITY_1D, IDCT_1D,
  IIDENTITY_1D, IADST_1D,     IIDENTITY_1D, IFLIPADST_1D,
};

// 1D functions
static const transform_1d_neon lowbd_txfm_all_1d_arr[TX_SIZES][ITX_TYPES_1D] = {
  { av1_idct4_new, av1_iadst4_new, av1_iidentity4_c },
  { av1_idct8_new, av1_iadst8_new, av1_iidentity8_c },
  { av1_idct16_new, av1_iadst16_new, av1_iidentity16_c },
  { av1_idct32_new, NULL, NULL },
  { av1_idct64_new, NULL, NULL },
};

static INLINE void lowbd_add_flip_buffer_8xn_neon(int16x8_t *in,
                                                  uint8_t *output, int stride,
                                                  int flipud,
                                                  const int height) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  int16x8_t temp_output;
  for (int i = 0; i < height; ++i, j += step) {
    temp_output = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(output)));
    temp_output = vaddq_s16(temp_output, in[j]);
    vst1_u8(output, vqmovun_s16(temp_output));
    output += stride;
  }
}

static INLINE uint8x16_t lowbd_get_recon_16x16_neon(const uint8x16_t pred,
                                                    int16x8_t res0,
                                                    int16x8_t res1) {
  int16x8_t temp_output[2];
  uint8x16_t temp_output_8q;
  temp_output[0] = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(pred)));
  temp_output[0] = vaddq_s16(temp_output[0], res0);
  temp_output[1] = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(pred)));
  temp_output[1] = vaddq_s16(temp_output[1], res1);
  temp_output_8q =
      vcombine_u8(vqmovun_s16(temp_output[0]), vqmovun_s16(temp_output[1]));
  return temp_output_8q;
}

static INLINE void lowbd_add_flip_buffer_16xn_neon(int16x8_t *in,
                                                   uint8_t *output, int stride,
                                                   int flipud, int height) {
  uint8x16_t temp_output_8q[32];
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    temp_output_8q[i] = vld1q_u8(output + i * stride);
    temp_output_8q[i] =
        lowbd_get_recon_16x16_neon(temp_output_8q[i], in[j], in[j + height]);
    vst1q_u8((output + i * stride), temp_output_8q[i]);
  }
}

static INLINE void dct_const_round_shift_low_8_dual(const int32x4_t *const t32,
                                                    int16x8_t *const d0,
                                                    int16x8_t *const d1,
                                                    int8_t cos_bit) {
  const int32x4_t dup_cos_bits_n_32x4 = vdupq_n_s32(-cos_bit);
  *d0 = vcombine_s16(vmovn_s32(vrshlq_s32(t32[0], dup_cos_bits_n_32x4)),
                     vmovn_s32(vrshlq_s32(t32[1], dup_cos_bits_n_32x4)));
  *d1 = vcombine_s16(vmovn_s32(vrshlq_s32(t32[2], dup_cos_bits_n_32x4)),
                     vmovn_s32(vrshlq_s32(t32[3], dup_cos_bits_n_32x4)));
}

static INLINE void btf_16_lane_0_1_neon(const int16x8_t in0,
                                        const int16x8_t in1, const int16x4_t c,
                                        int16x8_t *t0, int16x8_t *t1) {
  int32x4_t s0[2], s1[2];
  int16x4_t v0[2], v1[2];

  s0[0] = vmull_lane_s16(vget_low_s16(in0), c, 0);
  s0[1] = vmull_lane_s16(vget_high_s16(in0), c, 0);
  s1[0] = vmull_lane_s16(vget_low_s16(in0), c, 1);
  s1[1] = vmull_lane_s16(vget_high_s16(in0), c, 1);

  s0[0] = vmlal_lane_s16(s0[0], vget_low_s16(in1), c, 1);
  s0[1] = vmlal_lane_s16(s0[1], vget_high_s16(in1), c, 1);
  s1[0] = vmlsl_lane_s16(s1[0], vget_low_s16(in1), c, 0);
  s1[1] = vmlsl_lane_s16(s1[1], vget_high_s16(in1), c, 0);

  v0[0] = vrshrn_n_s32(s0[0], INV_COS_BIT);
  v0[1] = vrshrn_n_s32(s0[1], INV_COS_BIT);
  v1[0] = vrshrn_n_s32(s1[0], INV_COS_BIT);
  v1[1] = vrshrn_n_s32(s1[1], INV_COS_BIT);

  *t0 = vcombine_s16(v0[0], v0[1]);
  *t1 = vcombine_s16(v1[0], v1[1]);
}

static INLINE void btf_16_lane_2_3_neon(const int16x8_t in0,
                                        const int16x8_t in1, const int16x4_t c,
                                        int16x8_t *t0, int16x8_t *t1) {
  int32x4_t s0[2], s1[2];
  int16x4_t v0[2], v1[2];

  s0[0] = vmull_lane_s16(vget_low_s16(in0), c, 2);
  s0[1] = vmull_lane_s16(vget_high_s16(in0), c, 2);
  s1[0] = vmull_lane_s16(vget_low_s16(in0), c, 3);
  s1[1] = vmull_lane_s16(vget_high_s16(in0), c, 3);

  s0[0] = vmlal_lane_s16(s0[0], vget_low_s16(in1), c, 3);
  s0[1] = vmlal_lane_s16(s0[1], vget_high_s16(in1), c, 3);
  s1[0] = vmlsl_lane_s16(s1[0], vget_low_s16(in1), c, 2);
  s1[1] = vmlsl_lane_s16(s1[1], vget_high_s16(in1), c, 2);

  v0[0] = vrshrn_n_s32(s0[0], INV_COS_BIT);
  v0[1] = vrshrn_n_s32(s0[1], INV_COS_BIT);
  v1[0] = vrshrn_n_s32(s1[0], INV_COS_BIT);
  v1[1] = vrshrn_n_s32(s1[1], INV_COS_BIT);

  *t0 = vcombine_s16(v0[0], v0[1]);
  *t1 = vcombine_s16(v1[0], v1[1]);
}

static INLINE void btf_16_neon(const int16x8_t in0, int16_t coef1,
                               int16_t coef2, int16x8_t *t0, int16x8_t *t1) {
  int32x4_t s0_l, s0_h, s1_l, s1_h;
  int16x4_t v0[2], v1[2];

  s0_l = vmull_n_s16(vget_low_s16(in0), coef1);
  s0_h = vmull_n_s16(vget_high_s16(in0), coef1);
  s1_l = vmull_n_s16(vget_low_s16(in0), -coef2);
  s1_h = vmull_n_s16(vget_high_s16(in0), -coef2);

  v0[0] = vrshrn_n_s32(s0_l, INV_COS_BIT);
  v0[1] = vrshrn_n_s32(s0_h, INV_COS_BIT);
  v1[0] = vrshrn_n_s32(s1_l, INV_COS_BIT);
  v1[1] = vrshrn_n_s32(s1_h, INV_COS_BIT);

  *t0 = vcombine_s16(v0[0], v0[1]);
  *t1 = vcombine_s16(v1[0], v1[1]);
}

static INLINE void btf_16_lane_3_2_neon(const int16x8_t in0,
                                        const int16x8_t in1, const int16x4_t c,
                                        int16x8_t *t0, int16x8_t *t1) {
  int32x4_t s0[2], s1[2];
  int16x4_t v0[2], v1[2];

  s0[0] = vmull_lane_s16(vget_low_s16(in0), c, 3);
  s0[1] = vmull_lane_s16(vget_high_s16(in0), c, 3);
  s1[0] = vmull_lane_s16(vget_low_s16(in0), c, 2);
  s1[1] = vmull_lane_s16(vget_high_s16(in0), c, 2);

  s0[0] = vmlal_lane_s16(s0[0], vget_low_s16(in1), c, 2);
  s0[1] = vmlal_lane_s16(s0[1], vget_high_s16(in1), c, 2);
  s1[0] = vmlsl_lane_s16(s1[0], vget_low_s16(in1), c, 3);
  s1[1] = vmlsl_lane_s16(s1[1], vget_high_s16(in1), c, 3);

  v0[0] = vrshrn_n_s32(s0[0], INV_COS_BIT);
  v0[1] = vrshrn_n_s32(s0[1], INV_COS_BIT);
  v1[0] = vrshrn_n_s32(s1[0], INV_COS_BIT);
  v1[1] = vrshrn_n_s32(s1[1], INV_COS_BIT);

  *t0 = vcombine_s16(v0[0], v0[1]);
  *t1 = vcombine_s16(v1[0], v1[1]);
}

static INLINE void btf_16_half_neon(int16x8_t *const x, const int16x4_t c) {
  int32x4_t t0[2], t1[2];
  int16x4_t v0[2], v1[2];

  // Don't add/sub before multiply, which will overflow in iadst8.
  const int32x4_t x0_lo = vmull_lane_s16(vget_low_s16(x[0]), c, 0);
  const int32x4_t x0_hi = vmull_lane_s16(vget_high_s16(x[0]), c, 0);
  const int32x4_t x1_lo = vmull_lane_s16(vget_low_s16(x[1]), c, 0);
  const int32x4_t x1_hi = vmull_lane_s16(vget_high_s16(x[1]), c, 0);

  t0[0] = vaddq_s32(x0_lo, x1_lo);
  t0[1] = vaddq_s32(x0_hi, x1_hi);
  t1[0] = vsubq_s32(x0_lo, x1_lo);
  t1[1] = vsubq_s32(x0_hi, x1_hi);

  v0[0] = vrshrn_n_s32(t0[0], INV_COS_BIT);
  v0[1] = vrshrn_n_s32(t0[1], INV_COS_BIT);
  v1[0] = vrshrn_n_s32(t1[0], INV_COS_BIT);
  v1[1] = vrshrn_n_s32(t1[1], INV_COS_BIT);

  x[0] = vcombine_s16(v0[0], v0[1]);
  x[1] = vcombine_s16(v1[0], v1[1]);
}

static INLINE int16x4_t create_s16x4_neon(int16_t *const c0, int16_t *const c1,
                                          int16_t *const c2,
                                          int16_t *const c3) {
  int16x4_t val = { 0 };
  val = vld1_lane_s16(c0, val, 0);
  val = vld1_lane_s16(c1, val, 1);
  val = vld1_lane_s16(c2, val, 2);
  val = vld1_lane_s16(c3, val, 3);
  return val;
}

static INLINE void iadst8_new_neon(int16x8_t *const in, int16x8_t *out,
                                   int8_t cos_bit, int bit) {
  (void)bit;
  const int32_t *cospi = cospi_arr(cos_bit);

  const int16x4_t c0 =
      create_s16x4_neon((int16_t *)(cospi + 4), (int16_t *)(cospi + 60),
                        (int16_t *)(cospi + 20), (int16_t *)(cospi + 44));
  const int16x4_t c1 =
      create_s16x4_neon((int16_t *)(cospi + 36), (int16_t *)(cospi + 28),
                        (int16_t *)(cospi + 52), (int16_t *)(cospi + 12));
  const int16x4_t c2 =
      create_s16x4_neon((int16_t *)(cospi + 32), (int16_t *)(cospi + 32),
                        (int16_t *)(cospi + 16), (int16_t *)(cospi + 48));

  int16x8_t x[8];
  int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;

  // Stage 1
  x[0] = in[7];
  x[1] = in[0];
  x[2] = in[5];
  x[3] = in[2];
  x[4] = in[3];
  x[5] = in[4];
  x[6] = in[1];
  x[7] = in[6];

  // Stage 2
  btf_16_lane_0_1_neon(x[0], x[1], c0, &s0, &s1);
  btf_16_lane_2_3_neon(x[2], x[3], c0, &s2, &s3);
  btf_16_lane_0_1_neon(x[4], x[5], c1, &s4, &s5);
  btf_16_lane_2_3_neon(x[6], x[7], c1, &s6, &s7);

  // Stage 3
  x[0] = vqaddq_s16(s0, s4);
  x[1] = vqaddq_s16(s1, s5);
  x[2] = vqaddq_s16(s2, s6);
  x[3] = vqaddq_s16(s3, s7);
  x[4] = vqsubq_s16(s0, s4);
  x[5] = vqsubq_s16(s1, s5);
  x[6] = vqsubq_s16(s2, s6);
  x[7] = vqsubq_s16(s3, s7);

  // Stage 4
  s0 = x[0];
  s1 = x[1];
  s2 = x[2];
  s3 = x[3];
  btf_16_lane_2_3_neon(x[4], x[5], c2, &s4, &s5);
  btf_16_lane_3_2_neon(x[7], x[6], c2, &s7, &s6);

  // Stage 5
  x[0] = vqaddq_s16(s0, s2);
  x[1] = vqaddq_s16(s1, s3);
  x[2] = vqsubq_s16(s0, s2);
  x[3] = vqsubq_s16(s1, s3);
  x[4] = vqaddq_s16(s4, s6);
  x[5] = vqaddq_s16(s5, s7);
  x[6] = vqsubq_s16(s4, s6);
  x[7] = vqsubq_s16(s5, s7);

  // stage 6
  btf_16_half_neon(x + 2, c2);
  btf_16_half_neon(x + 6, c2);

  // Stage 7
  out[0] = x[0];
  out[1] = vnegq_s16(x[4]);
  out[2] = x[6];
  out[3] = vnegq_s16(x[2]);
  out[4] = x[3];
  out[5] = vnegq_s16(x[7]);
  out[6] = x[5];
  out[7] = vnegq_s16(x[1]);
}

static INLINE void iadst8_low1_new_neon(int16x8_t *const in, int16x8_t *out,
                                        int8_t cos_bit, int bit) {
  (void)bit;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int16x4_t c2 =
      create_s16x4_neon((int16_t *)(cospi + 32), (int16_t *)(cospi + 32),
                        (int16_t *)(cospi + 16), (int16_t *)(cospi + 48));

  int16x8_t x[8] = { { 0 } };
  int16x8_t s0, s1, s4, s5;

  // Stage 1
  x[1] = in[0];

  // Stage 2

  btf_16_neon(x[1], cospi[60], cospi[4], &s0, &s1);

  // Stage 3
  x[0] = s0;
  x[1] = s1;
  x[4] = s0;
  x[5] = s1;

  // Stage 4
  s0 = x[0];
  s1 = x[1];
  btf_16_lane_2_3_neon(x[4], x[5], c2, &s4, &s5);

  // Stage 5
  x[0] = s0;
  x[1] = s1;
  x[2] = s0;
  x[3] = s1;
  x[4] = s4;
  x[5] = s5;
  x[6] = s4;
  x[7] = s5;

  // stage 6
  btf_16_half_neon(x + 2, c2);
  btf_16_half_neon(x + 6, c2);

  // Stage 7
  out[0] = x[0];
  out[1] = vnegq_s16(x[4]);
  out[2] = x[6];
  out[3] = vnegq_s16(x[2]);
  out[4] = x[3];
  out[5] = vnegq_s16(x[7]);
  out[6] = x[5];
  out[7] = vnegq_s16(x[1]);
}

static INLINE void idct8_new_neon(int16x8_t *in, int16x8_t *out, int8_t cos_bit,
                                  int bit) {
  (void)bit;
  const int32_t *cospi = cospi_arr(cos_bit);
  int16x8_t step1[8], step2[8];
  const int16x4_t c0 =
      create_s16x4_neon((int16_t *)(cospi + 8), (int16_t *)(cospi + 56),
                        (int16_t *)(cospi + 40), (int16_t *)(cospi + 24));
  const int16x4_t c2 =
      create_s16x4_neon((int16_t *)(cospi + 32), (int16_t *)(cospi + 32),
                        (int16_t *)(cospi + 16), (int16_t *)(cospi + 48));

  // stage 2
  btf_16_lane_0_1_neon(in[1], in[7], c0, &step1[7], &step1[4]);
  btf_16_lane_2_3_neon(in[5], in[3], c0, &step1[6], &step1[5]);

  // stage 3
  btf_16_lane_0_1_neon(in[0], in[4], c2, &step2[0], &step2[1]);
  btf_16_lane_2_3_neon(in[2], in[6], c2, &step2[3], &step2[2]);
  step2[4] = vqaddq_s16(step1[4], step1[5]);
  step2[5] = vqsubq_s16(step1[4], step1[5]);
  step2[6] = vqsubq_s16(step1[7], step1[6]);
  step2[7] = vqaddq_s16(step1[7], step1[6]);

  // stage 4
  step1[0] = vqaddq_s16(step2[0], step2[3]);
  step1[1] = vqaddq_s16(step2[1], step2[2]);
  step1[2] = vqsubq_s16(step2[1], step2[2]);
  step1[3] = vqsubq_s16(step2[0], step2[3]);
  btf_16_lane_0_1_neon(step2[6], step2[5], c2, &step1[6], &step1[5]);

  // stage 5
  out[0] = vqaddq_s16(step1[0], step2[7]);
  out[1] = vqaddq_s16(step1[1], step1[6]);
  out[2] = vqaddq_s16(step1[2], step1[5]);
  out[3] = vqaddq_s16(step1[3], step2[4]);
  out[4] = vqsubq_s16(step1[3], step2[4]);
  out[5] = vqsubq_s16(step1[2], step1[5]);
  out[6] = vqsubq_s16(step1[1], step1[6]);
  out[7] = vqsubq_s16(step1[0], step2[7]);
}

static INLINE void idct8_low1_new_neon(int16x8_t *in, int16x8_t *out,
                                       int8_t cos_bit, int bit) {
  (void)bit;
  const int32_t *cospi = cospi_arr(cos_bit);
  int16x4_t step1l[4], step1h[4];
  int16x8_t step1[8], step2[8];
  int32x4_t t32[8];

  // stage 1
  step1l[0] = vget_low_s16(in[0]);
  step1h[0] = vget_high_s16(in[0]);

  // stage 2
  t32[2] = vmull_n_s16(step1l[0], (int16_t)cospi[32]);
  t32[3] = vmull_n_s16(step1h[0], (int16_t)cospi[32]);

  t32[0] = t32[2];
  t32[1] = t32[3];
  dct_const_round_shift_low_8_dual(&t32[0], &step2[0], &step2[1], cos_bit);

  // stage 3
  step1[0] = step2[0];
  step1[1] = step2[1];
  step1[2] = step2[1];
  step1[3] = step2[0];

  // stage 4
  out[0] = step1[0];
  out[1] = step1[1];
  out[2] = step1[2];
  out[3] = step1[3];
  out[4] = step1[3];
  out[5] = step1[2];
  out[6] = step1[1];
  out[7] = step1[0];
}

void av1_round_shift_array_16_neon(int16x8_t *arr, int size, int bit) {
  assert(!(size % 4));
  if (!bit) return;
  const int16x8_t dup_bits_n_16x8 = vdupq_n_s16((int16_t)(-bit));
  for (int i = 0; i < size; i++) {
    arr[i] = vrshlq_s16(arr[i], dup_bits_n_16x8);
  }
}

static INLINE void flip_buf_ud_neon(int16x8_t *input, int size) {
  int16x8_t temp[8];
  for (int i = 0; i < size; ++i) {
    temp[i] = input[size - 1 - i];
  }
  for (int i = 0; i < size; ++i) {
    input[i] = temp[i];
  }
}

static INLINE void load_buffer_32bit_to_16bit_neon(const int32_t *input,
                                                   int16x8_t *const a,
                                                   int out_size) {
  for (int i = 0; i < 8; ++i) {
    a[i] = vcombine_s16(vmovn_s32(vld1q_s32(input)),
                        vmovn_s32(vld1q_s32(input + 4)));
    input += out_size;
  }
}

static INLINE void av1_identity8_new_neon(int16x8_t *input, int16x8_t *output,
                                          int8_t cos_bit, int bit) {
  (void)bit;
  (void)cos_bit;

  output[0] = vmulq_n_s16(input[0], (int16_t)2);
  output[1] = vmulq_n_s16(input[1], (int16_t)2);
  output[2] = vmulq_n_s16(input[2], (int16_t)2);
  output[3] = vmulq_n_s16(input[3], (int16_t)2);
  output[4] = vmulq_n_s16(input[4], (int16_t)2);
  output[5] = vmulq_n_s16(input[5], (int16_t)2);
  output[6] = vmulq_n_s16(input[6], (int16_t)2);
  output[7] = vmulq_n_s16(input[7], (int16_t)2);
}

// Capitalize Functions for blocks with eob at DC and within
// topleft 8x8, 16x16, 32x32 corner
static const transform_1d_neon
    lowbd_txfm_all_1d_zeros_w8_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { av1_idct4_new, av1_idct4_new, NULL, NULL },
          { av1_iadst4_new, av1_iadst4_new, NULL, NULL },
          { av1_iidentity4_c, av1_iidentity4_c, NULL, NULL },
      },
      { { av1_idct8_new, av1_idct8_new, NULL, NULL },
        { av1_iadst8_new, av1_iadst8_new, NULL, NULL },
        { av1_iidentity8_c, av1_iidentity8_c, NULL, NULL } },
      {
          { av1_idct16_new, av1_idct16_new, av1_idct16_new, NULL },
          { av1_iadst16_new, av1_iadst16_new, av1_iadst16_new, NULL },
          { av1_iidentity16_c, av1_iidentity16_c, av1_iidentity16_c, NULL },
      },
      { { av1_idct32_new, av1_idct32_new, av1_idct32_new, av1_idct32_new },
        { NULL, NULL, NULL, NULL },
        { av1_iidentity32_c, av1_iidentity32_c, av1_iidentity32_c,
          av1_iidentity32_c } },
      { { av1_idct64_new, av1_idct64_new, av1_idct64_new, av1_idct64_new },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };

static const transform_neon
    lowbd_txfm_all_1d_zeros_w_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { idct8_low1_new_neon, idct8_new_neon, NULL, NULL },
        { iadst8_low1_new_neon, iadst8_new_neon, NULL, NULL },
        { av1_identity8_new_neon, av1_identity8_new_neon, NULL, NULL } },
      {
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },
      { { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };

static INLINE void lowbd_inv_txfm2d_add_wxh_idtx_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  DECLARE_ALIGNED(32, int, txfm_buf[32 * 32 + 32 + 32]);
  int32_t *temp_in = txfm_buf;

  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;

  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);

  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);

  // row tx
  int row_start = (buf_size_nonzero_h_div8 * 8);
  for (int i = 0; i < row_start; i++) {
    if (abs(rect_type) == 1) {
      for (int j = 0; j < txfm_size_col; j++)
        temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);
      row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    } else {
      row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  // Doing memset for the rows which are not processed in row transform.
  memset(buf_ptr, 0,
         sizeof(int32_t) * txfm_size_col * (txfm_size_row - row_start));

  // col tx
  for (int c = 0; c < txfm_size_col; c++) {
    for (r = 0; r < txfm_size_row; ++r) temp_in[r] = buf[r * txfm_size_col + c];

    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    for (r = 0; r < txfm_size_row; ++r) {
      output[r * stride + c] =
          highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
    }
  }
}

static INLINE void lowbd_inv_txfm2d_add_idtx_neon(const int32_t *input,
                                                  uint8_t *output, int stride,
                                                  TX_TYPE tx_type,
                                                  TX_SIZE tx_size, int eob) {
  int16x8_t a[64];
  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int buf_size_nonzero_w_div8 = (eobx + 8) >> 3;
  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const int32_t *input_1;
  const transform_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);

  for (int i = 0; i < buf_size_nonzero_h_div8; i++) {
    input_1 = input;
    for (int j = 0; j < buf_size_nonzero_w_div8; ++j) {
      int k = j * 8 + i * txfm_size_col;
      load_buffer_32bit_to_16bit_neon(input_1, &a[k], txfm_size_col);
      input_1 += 8;
    }
    input += (txfm_size_col * 8);
    row_txfm(&a[i * txfm_size_col], &a[i * txfm_size_col], cos_bit_row, 0);
    av1_round_shift_array_16_neon(&a[i * txfm_size_col], txfm_size_col,
                                  -shift[0]);
  }
  for (int j = 0; j < buf_size_w_div8; ++j) {
    col_txfm(&a[j * txfm_size_row], &a[j * txfm_size_row], cos_bit_col, 0);
    av1_round_shift_array_16_neon(&a[j * txfm_size_row], txfm_size_row,
                                  -shift[1]);
  }
  if (txfm_size_col >= 16) {
    for (int i = 0; i < (txfm_size_col >> 4); i++) {
      lowbd_add_flip_buffer_16xn_neon(
          &a[i * txfm_size_row * 2], output + 16 * i, stride, 0, txfm_size_row);
    }
  } else if (txfm_size_col == 8) {
    lowbd_add_flip_buffer_8xn_neon(a, output, stride, 0, txfm_size_row);
  }
}

static INLINE void lowbd_inv_txfm2d_add_v_wxh_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  DECLARE_ALIGNED(32, int, txfm_buf[32 * 32 + 32 + 32]);
  int32_t *temp_in = txfm_buf;

  int eobx, eoby;
  get_eobx_eoby_scan_v_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;

  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);

  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // row tx
  int row_start = (buf_size_nonzero_h_div8 * 8);
  for (int i = 0; i < row_start; i++) {
    if (abs(rect_type) == 1) {
      for (int j = 0; j < txfm_size_col; j++)
        temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);
      row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    } else {
      row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }
  // Doing memset for the rows which are not processed in row transform.
  memset(buf_ptr, 0,
         sizeof(int32_t) * txfm_size_col * (txfm_size_row - row_start));

  // col tx
  for (int c = 0; c < txfm_size_col; c++) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

static INLINE void lowbd_inv_txfm2d_add_v_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  int16x8_t a[64];
  int16x8_t b[64];
  int eobx, eoby, ud_flip, lr_flip;
  get_eobx_eoby_scan_v_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int buf_size_nonzero_w_div8 = (eobx + 8) >> 3;
  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const int32_t *input_1;
  int temp_b = 0;
  const transform_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < buf_size_nonzero_h_div8; i++) {
    input_1 = input;
    for (int j = 0; j < buf_size_nonzero_w_div8; ++j) {
      int k = j * 8 + i * txfm_size_col;
      load_buffer_32bit_to_16bit_neon(input_1, &a[k], txfm_size_col);
      transpose_s16_8x8q(&a[k], &a[k]);
      input_1 += 8;
    }
    input += (txfm_size_col * 8);
    row_txfm(&a[i * txfm_size_col], &a[i * txfm_size_col], cos_bit_row, 0);
    av1_round_shift_array_16_neon(&a[i * txfm_size_col], txfm_size_col,
                                  -shift[0]);
    if (lr_flip == 1) {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        int k = j * 8 + i * txfm_size_col;
        flip_buf_ud_neon(&a[k], 8);
        transpose_s16_8x8q(
            &a[k], &b[temp_b + txfm_size_row * (buf_size_w_div8 - 1 - j)]);
      }
      temp_b += 8;
    } else {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        int k = j * 8 + i * txfm_size_col;
        transpose_s16_8x8q(&a[k], &b[temp_b + txfm_size_row * j]);
      }
      temp_b += 8;
    }
  }
  for (int j = 0; j < buf_size_w_div8; ++j) {
    col_txfm(&b[j * txfm_size_row], &b[j * txfm_size_row], cos_bit_col, 0);
    av1_round_shift_array_16_neon(&b[j * txfm_size_row], txfm_size_row,
                                  -shift[1]);
  }
  if (txfm_size_col >= 16) {
    for (int i = 0; i < (txfm_size_col >> 4); i++) {
      lowbd_add_flip_buffer_16xn_neon(
          &b[i * txfm_size_row * 2], output + 16 * i, stride, 0, txfm_size_row);
    }
  } else if (txfm_size_col == 8) {
    lowbd_add_flip_buffer_8xn_neon(b, output, stride, 0, txfm_size_row);
  }
}

static INLINE void lowbd_inv_txfm2d_add_h_wxh_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  DECLARE_ALIGNED(32, int, txfm_buf[32 * 32 + 32 + 32]);
  int32_t *temp_in = txfm_buf;

  int eobx, eoby;
  get_eobx_eoby_scan_h_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;

  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);

  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // row tx
  int row_start = (buf_size_nonzero_h_div8 * 8);
  for (int i = 0; i < row_start; i++) {
    if (abs(rect_type) == 1) {
      for (int j = 0; j < txfm_size_col; j++)
        temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);
      row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    } else {
      row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }
  // Doing memset for the rows which are not processed in row transform.
  memset(buf_ptr, 0,
         sizeof(int32_t) * txfm_size_col * (txfm_size_row - row_start));

  // col tx
  for (int c = 0; c < txfm_size_col; c++) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

static INLINE void lowbd_inv_txfm2d_add_h_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  int16x8_t a[64];
  int eobx, eoby, ud_flip, lr_flip;
  get_eobx_eoby_scan_h_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int buf_size_nonzero_w_div8 = (eobx + 8) >> 3;
  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const int32_t *input_1;
  const transform_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < buf_size_nonzero_h_div8; i++) {
    input_1 = input;
    for (int j = 0; j < buf_size_nonzero_w_div8; ++j) {
      int k = j * 8 + i * txfm_size_col;
      load_buffer_32bit_to_16bit_neon(input_1, &a[k], txfm_size_col);
      input_1 += 8;
    }
    input += (txfm_size_col * 8);
    row_txfm(&a[i * txfm_size_col], &a[i * txfm_size_col], cos_bit_row, 0);
    av1_round_shift_array_16_neon(&a[i * txfm_size_col], txfm_size_col,
                                  -shift[0]);
  }
  for (int j = 0; j < buf_size_w_div8; ++j) {
    col_txfm(&a[j * txfm_size_row], &a[j * txfm_size_row], cos_bit_col, 0);
    av1_round_shift_array_16_neon(&a[j * txfm_size_row], txfm_size_row,
                                  -shift[1]);
  }
  if (txfm_size_col >= 16) {
    for (int i = 0; i < (txfm_size_col >> 4); i++) {
      lowbd_add_flip_buffer_16xn_neon(&a[i * txfm_size_row * 2],
                                      output + 16 * i, stride, ud_flip,
                                      txfm_size_row);
    }
  } else if (txfm_size_col == 8) {
    lowbd_add_flip_buffer_8xn_neon(a, output, stride, ud_flip, txfm_size_row);
  }
}

static INLINE void lowbd_inv_txfm2d_add_4x4_neon(const int32_t *input,
                                                 uint8_t *output, int stride,
                                                 TX_TYPE tx_type,
                                                 TX_SIZE tx_size, int eob) {
  (void)eob;
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 4 + 8 + 8]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    row_txfm(input, buf_ptr, cos_bit_row, stage_range);

    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

void lowbd_inv_txfm2d_add_4x8_neon(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob) {
  (void)eob;
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 8 + 8 + 8]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    for (int j = 0; j < txfm_size_col; j++)
      temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);

    row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

void lowbd_inv_txfm2d_add_8x4_neon(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob) {
  (void)eob;
  DECLARE_ALIGNED(32, int, txfm_buf[8 * 4 + 8 + 8]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    for (int j = 0; j < txfm_size_col; j++)
      temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);

    row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

void lowbd_inv_txfm2d_add_4x16_neon(const int32_t *input, uint8_t *output,
                                    int stride, TX_TYPE tx_type,
                                    TX_SIZE tx_size, int eob) {
  (void)eob;
  DECLARE_ALIGNED(32, int, txfm_buf[4 * 16 + 16 + 16]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

void lowbd_inv_txfm2d_add_16x4_neon(const int32_t *input, uint8_t *output,
                                    int stride, TX_TYPE tx_type,
                                    TX_SIZE tx_size, int eob) {
  (void)eob;

  DECLARE_ALIGNED(32, int, txfm_buf[16 * 4 + 16 + 16]);
  int32_t *temp_in = txfm_buf;

  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);
  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  int r, bd = 8;
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < txfm_size_row; i++) {
    row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  for (int c = 0; c < txfm_size_col; ++c) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

static INLINE void lowbd_inv_txfm2d_add_wxh_no_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  DECLARE_ALIGNED(32, int, txfm_buf[64 * 64 + 64 + 64]);
  int32_t *temp_in = txfm_buf;

  int eobx, eoby, ud_flip, lr_flip, row_start;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int buf_offset = AOMMAX(txfm_size_row, txfm_size_col);

  int32_t *temp_out = temp_in + buf_offset;
  int32_t *buf = temp_out + buf_offset;
  int32_t *buf_ptr = buf;
  const int8_t stage_range[MAX_TXFM_STAGE_NUM] = { 16 };
  const int bd = 8;
  int r;

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  row_start = (buf_size_nonzero_h_div8 << 3);

  for (int i = 0; i < row_start; i++) {
    if (abs(rect_type) == 1) {
      for (int j = 0; j < txfm_size_col; j++)
        temp_in[j] = round_shift((int64_t)input[j] * NewInvSqrt2, NewSqrt2Bits);
      row_txfm(temp_in, buf_ptr, cos_bit_row, stage_range);
    } else {
      row_txfm(input, buf_ptr, cos_bit_row, stage_range);
    }
    av1_round_shift_array(buf_ptr, txfm_size_col, -shift[0]);
    input += txfm_size_col;
    buf_ptr += txfm_size_col;
  }

  // Doing memset for the rows which are not processed in row transform.
  memset(buf_ptr, 0,
         sizeof(int32_t) * txfm_size_col * (txfm_size_row - row_start));

  for (int c = 0; c < txfm_size_col; c++) {
    if (lr_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + c];
    } else {
      // flip left right
      for (r = 0; r < txfm_size_row; ++r)
        temp_in[r] = buf[r * txfm_size_col + (txfm_size_col - c - 1)];
    }
    col_txfm(temp_in, temp_out, cos_bit_col, stage_range);
    av1_round_shift_array(temp_out, txfm_size_row, -shift[1]);

    if (ud_flip == 0) {
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] =
            highbd_clip_pixel_add(output[r * stride + c], temp_out[r], bd);
      }
    } else {
      // flip upside down
      for (r = 0; r < txfm_size_row; ++r) {
        output[r * stride + c] = highbd_clip_pixel_add(
            output[r * stride + c], temp_out[txfm_size_row - r - 1], bd);
      }
    }
  }
}

static INLINE void lowbd_inv_txfm2d_add_no_identity_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  int16x8_t a[64];
  int16x8_t b[64];
  int eobx, eoby, ud_flip, lr_flip;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int buf_size_nonzero_w_div8 = (eobx + 8) >> 3;
  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const int32_t *input_1;
  int temp_b = 0;

  const transform_neon row_txfm =
      lowbd_txfm_all_1d_zeros_w_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_neon col_txfm =
      lowbd_txfm_all_1d_zeros_w_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < buf_size_nonzero_h_div8; i++) {
    input_1 = input;
    for (int j = 0; j < buf_size_nonzero_w_div8; ++j) {
      int k = j * 8 + i * txfm_size_col;
      load_buffer_32bit_to_16bit_neon(input_1, &a[k], txfm_size_col);
      transpose_s16_8x8q(&a[k], &a[k]);
      input_1 += 8;
    }
    input += (txfm_size_col * 8);
    row_txfm(&a[i * txfm_size_col], &a[i * txfm_size_col], cos_bit_row, 0);
    av1_round_shift_array_16_neon(&a[i * txfm_size_col], txfm_size_col,
                                  -shift[0]);
    if (lr_flip == 1) {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        int k = j * 8 + i * txfm_size_col;
        flip_buf_ud_neon(&a[k], 8);
        transpose_s16_8x8q(
            &a[k], &b[temp_b + txfm_size_row * (buf_size_w_div8 - 1 - j)]);
      }
      temp_b += 8;
    } else {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        int k = j * 8 + i * txfm_size_col;
        transpose_s16_8x8q(&a[k], &b[temp_b + txfm_size_row * j]);
      }
      temp_b += 8;
    }
  }
  for (int j = 0; j < buf_size_w_div8; ++j) {
    col_txfm(&b[j * txfm_size_row], &b[j * txfm_size_row], cos_bit_col, 0);
    av1_round_shift_array_16_neon(&b[j * txfm_size_row], txfm_size_row,
                                  -shift[1]);
  }

  if (txfm_size_col >= 16) {
    for (int i = 0; i < (txfm_size_col >> 4); i++) {
      lowbd_add_flip_buffer_16xn_neon(&b[i * txfm_size_row * 2],
                                      output + 16 * i, stride, ud_flip,
                                      txfm_size_row);
    }
  } else if (txfm_size_col == 8) {
    lowbd_add_flip_buffer_8xn_neon(b, output, stride, ud_flip, txfm_size_row);
  }
}

static INLINE void lowbd_inv_txfm2d_add_wxh_universe_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  switch (tx_type) {
    case IDTX:
      lowbd_inv_txfm2d_add_wxh_idtx_neon(input, output, stride, tx_type,
                                         tx_size, eob);
      break;

    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      lowbd_inv_txfm2d_add_v_wxh_identity_neon(input, output, stride, tx_type,
                                               tx_size, eob);
      break;

    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      lowbd_inv_txfm2d_add_h_wxh_identity_neon(input, output, stride, tx_type,
                                               tx_size, eob);
      break;

    default:
      lowbd_inv_txfm2d_add_wxh_no_identity_neon(input, output, stride, tx_type,
                                                tx_size, eob);
      break;
  }
}

static INLINE void lowbd_inv_txfm2d_add_universe_neon(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  switch (tx_type) {
    case IDTX:
      lowbd_inv_txfm2d_add_idtx_neon(input, output, stride, tx_type, tx_size,
                                     eob);
      break;

    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      lowbd_inv_txfm2d_add_v_identity_neon(input, output, stride, tx_type,
                                           tx_size, eob);
      break;

    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      lowbd_inv_txfm2d_add_h_identity_neon(input, output, stride, tx_type,
                                           tx_size, eob);
      break;

    default:
      lowbd_inv_txfm2d_add_no_identity_neon(input, output, stride, tx_type,
                                            tx_size, eob);
      break;
  }
}

void av1_lowbd_inv_txfm2d_add_neon(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob) {
  int row;
  switch (tx_size) {
    case TX_4X4:
      lowbd_inv_txfm2d_add_4x4_neon(input, output, stride, tx_type, tx_size,
                                    eob);
      break;

    case TX_4X8:
      lowbd_inv_txfm2d_add_4x8_neon(input, output, stride, tx_type, tx_size,
                                    eob);
      break;

    case TX_8X4:
      lowbd_inv_txfm2d_add_8x4_neon(input, output, stride, tx_type, tx_size,
                                    eob);
      break;

    case TX_4X16:
      lowbd_inv_txfm2d_add_4x16_neon(input, output, stride, tx_type, tx_size,
                                     eob);
      break;

    case TX_16X4:
      lowbd_inv_txfm2d_add_16x4_neon(input, output, stride, tx_type, tx_size,
                                     eob);
      break;

    case TX_16X64: {
      lowbd_inv_txfm2d_add_wxh_universe_neon(input, output, stride, tx_type,
                                             tx_size, eob);
    } break;

    case TX_64X16: {
      int32_t mod_input[64 * 16];
      for (row = 0; row < 16; ++row) {
        memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
        memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
      }
      lowbd_inv_txfm2d_add_wxh_universe_neon(mod_input, output, stride, tx_type,
                                             tx_size, eob);
    } break;

    case TX_32X64: {
      lowbd_inv_txfm2d_add_wxh_universe_neon(input, output, stride, tx_type,
                                             tx_size, eob);
    } break;

    case TX_64X32: {
      int32_t mod_input[64 * 32];
      for (row = 0; row < 32; ++row) {
        memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
        memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
      }
      lowbd_inv_txfm2d_add_wxh_universe_neon(mod_input, output, stride, tx_type,
                                             tx_size, eob);
    } break;

    case TX_64X64: {
      int32_t mod_input[64 * 64];
      for (row = 0; row < 32; ++row) {
        memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
        memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
      }
      lowbd_inv_txfm2d_add_wxh_universe_neon(mod_input, output, stride, tx_type,
                                             tx_size, eob);
    } break;

    case TX_8X8: {
      lowbd_inv_txfm2d_add_universe_neon(input, output, stride, tx_type,
                                         tx_size, eob);
    } break;

    default:
      lowbd_inv_txfm2d_add_wxh_universe_neon(input, output, stride, tx_type,
                                             tx_size, eob);
      break;
  }
}
void av1_inv_txfm_add_neon(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (!txfm_param->lossless) {
    av1_lowbd_inv_txfm2d_add_neon(dqcoeff, dst, stride, tx_type,
                                  txfm_param->tx_size, txfm_param->eob);
  } else {
    av1_inv_txfm_add_c(dqcoeff, dst, stride, txfm_param);
  }
}
