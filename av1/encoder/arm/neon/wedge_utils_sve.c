/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "aom_dsp/arm/dot_sve.h"
#include "aom_dsp/arm/sum_neon.h"
#include "av1/common/reconinter.h"

uint64_t av1_wedge_sse_from_residuals_sve(const int16_t *r1, const int16_t *d,
                                          const uint8_t *m, int N) {
  assert(N % 64 == 0);

  int64x2_t sse[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };

  int i = 0;
  do {
    int32x4_t sum[4];
    int16x8_t sum_s16[2];

    const int16x8_t r1_l = vld1q_s16(r1 + i);
    const int16x8_t r1_h = vld1q_s16(r1 + i + 8);
    const int16x8_t d_l = vld1q_s16(d + i);
    const int16x8_t d_h = vld1q_s16(d + i + 8);
    // The following three lines are a bit inelegant compared to using a pair
    // of vmovl_u8()... but it forces the compiler to emit a ZIP1, ZIP2 pair -
    // which can be executed in parallel with the subsequent SSHL instructions.
    // (SSHL can only be executed on half of the Neon pipes in modern Arm
    // cores, whereas ZIP1/2 can be executed on all of them.)
    const uint8x16x2_t m_u16 = vzipq_u8(vld1q_u8(m + i), vdupq_n_u8(0));
    const int16x8_t m_l = vreinterpretq_s16_u8(m_u16.val[0]);
    const int16x8_t m_h = vreinterpretq_s16_u8(m_u16.val[1]);

    sum[0] = vshll_n_s16(vget_low_s16(r1_l), WEDGE_WEIGHT_BITS);
    sum[1] = vshll_n_s16(vget_high_s16(r1_l), WEDGE_WEIGHT_BITS);
    sum[2] = vshll_n_s16(vget_low_s16(r1_h), WEDGE_WEIGHT_BITS);
    sum[3] = vshll_n_s16(vget_high_s16(r1_h), WEDGE_WEIGHT_BITS);

    sum[0] = vmlal_s16(sum[0], vget_low_s16(m_l), vget_low_s16(d_l));
    sum[1] = vmlal_s16(sum[1], vget_high_s16(m_l), vget_high_s16(d_l));
    sum[2] = vmlal_s16(sum[2], vget_low_s16(m_h), vget_low_s16(d_h));
    sum[3] = vmlal_s16(sum[3], vget_high_s16(m_h), vget_high_s16(d_h));

    sum_s16[0] = vcombine_s16(vqmovn_s32(sum[0]), vqmovn_s32(sum[1]));
    sum_s16[1] = vcombine_s16(vqmovn_s32(sum[2]), vqmovn_s32(sum[3]));

    sse[0] = aom_sdotq_s16(sse[0], sum_s16[0], sum_s16[0]);
    sse[1] = aom_sdotq_s16(sse[1], sum_s16[1], sum_s16[1]);

    i += 16;
  } while (i < N);

  uint64_t csse = (uint64_t)horizontal_add_s64x2(vaddq_s64(sse[0], sse[1]));
  return ROUND_POWER_OF_TWO(csse, 2 * WEDGE_WEIGHT_BITS);
}
