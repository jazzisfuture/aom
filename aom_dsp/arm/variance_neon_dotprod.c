/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_ports/mem.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

static INLINE void variance_4xh_neon_dotprod(const uint8_t *src, int src_stride,
                                             const uint8_t *ref, int ref_stride,
                                             int h, uint32_t *sse, int *sum) {
  uint32x4_t src_sum = vdupq_n_u32(0);
  uint32x4_t ref_sum = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s = load_unaligned_u8q(src, src_stride);
    uint8x16_t r = load_unaligned_u8q(ref, ref_stride);

    src_sum = vdotq_u32(src_sum, s, vdupq_n_u8(1));
    ref_sum = vdotq_u32(ref_sum, r, vdupq_n_u8(1));

    uint8x16_t abs_diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

    src += 4 * src_stride;
    ref += 4 * ref_stride;
    i -= 4;
  } while (i != 0);

  int32x4_t sum_diff =
      vsubq_s32(vreinterpretq_s32_u32(src_sum), vreinterpretq_s32_u32(ref_sum));
  *sum = horizontal_add_s32x4(sum_diff);
  *sse = horizontal_add_u32x4(sse_u32);
}

static INLINE void variance_8xh_neon_dotprod(const uint8_t *src, int src_stride,
                                             const uint8_t *ref, int ref_stride,
                                             int h, uint32_t *sse, int *sum) {
  uint32x4_t src_sum = vdupq_n_u32(0);
  uint32x4_t ref_sum = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s = vcombine_u8(vld1_u8(src), vld1_u8(src + src_stride));
    uint8x16_t r = vcombine_u8(vld1_u8(ref), vld1_u8(ref + ref_stride));

    src_sum = vdotq_u32(src_sum, s, vdupq_n_u8(1));
    ref_sum = vdotq_u32(ref_sum, r, vdupq_n_u8(1));

    uint8x16_t abs_diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    i -= 2;
  } while (i != 0);

  int32x4_t sum_diff =
      vsubq_s32(vreinterpretq_s32_u32(src_sum), vreinterpretq_s32_u32(ref_sum));
  *sum = horizontal_add_s32x4(sum_diff);
  *sse = horizontal_add_u32x4(sse_u32);
}

static INLINE void variance_16xh_neon_dotprod(const uint8_t *src,
                                              int src_stride,
                                              const uint8_t *ref,
                                              int ref_stride, int h,
                                              uint32_t *sse, int *sum) {
  uint32x4_t src_sum = vdupq_n_u32(0);
  uint32x4_t ref_sum = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s = vld1q_u8(src);
    uint8x16_t r = vld1q_u8(ref);

    src_sum = vdotq_u32(src_sum, s, vdupq_n_u8(1));
    ref_sum = vdotq_u32(ref_sum, r, vdupq_n_u8(1));

    uint8x16_t abs_diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  int32x4_t sum_diff =
      vsubq_s32(vreinterpretq_s32_u32(src_sum), vreinterpretq_s32_u32(ref_sum));
  *sum = horizontal_add_s32x4(sum_diff);
  *sse = horizontal_add_u32x4(sse_u32);
}

static INLINE void variance_large_neon_dotprod(const uint8_t *src,
                                               int src_stride,
                                               const uint8_t *ref,
                                               int ref_stride, int w, int h,
                                               uint32_t *sse, int *sum) {
  uint32x4_t src_sum = vdupq_n_u32(0);
  uint32x4_t ref_sum = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    int j = 0;
    do {
      uint8x16_t s = vld1q_u8(src + j);
      uint8x16_t r = vld1q_u8(ref + j);

      src_sum = vdotq_u32(src_sum, s, vdupq_n_u8(1));
      ref_sum = vdotq_u32(ref_sum, r, vdupq_n_u8(1));

      uint8x16_t abs_diff = vabdq_u8(s, r);
      sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

      j += 16;
    } while (j < w);

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  int32x4_t sum_diff =
      vsubq_s32(vreinterpretq_s32_u32(src_sum), vreinterpretq_s32_u32(ref_sum));
  *sum = horizontal_add_s32x4(sum_diff);
  *sse = horizontal_add_u32x4(sse_u32);
}

static INLINE void variance_32xh_neon_dotprod(const uint8_t *src,
                                              int src_stride,
                                              const uint8_t *ref,
                                              int ref_stride, int h,
                                              uint32_t *sse, int *sum) {
  variance_large_neon_dotprod(src, src_stride, ref, ref_stride, 32, h, sse,
                              sum);
}

static INLINE void variance_64xh_neon_dotprod(const uint8_t *src,
                                              int src_stride,
                                              const uint8_t *ref,
                                              int ref_stride, int h,
                                              uint32_t *sse, int *sum) {
  variance_large_neon_dotprod(src, src_stride, ref, ref_stride, 64, h, sse,
                              sum);
}

static INLINE void variance_128xh_neon_dotprod(const uint8_t *src,
                                               int src_stride,
                                               const uint8_t *ref,
                                               int ref_stride, int h,
                                               uint32_t *sse, int *sum) {
  variance_large_neon_dotprod(src, src_stride, ref, ref_stride, 128, h, sse,
                              sum);
}

#define VARIANCE_WXH_NEON_DOTPROD(w, h, shift)                                \
  unsigned int aom_variance##w##x##h##_neon_dotprod(                          \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    int sum;                                                                  \
    variance_##w##xh_neon_dotprod(src, src_stride, ref, ref_stride, h, sse,   \
                                  &sum);                                      \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> shift);                  \
  }

VARIANCE_WXH_NEON_DOTPROD(4, 4, 4)
VARIANCE_WXH_NEON_DOTPROD(4, 8, 5)
VARIANCE_WXH_NEON_DOTPROD(4, 16, 6)

VARIANCE_WXH_NEON_DOTPROD(8, 4, 5)
VARIANCE_WXH_NEON_DOTPROD(8, 8, 6)
VARIANCE_WXH_NEON_DOTPROD(8, 16, 7)
VARIANCE_WXH_NEON_DOTPROD(8, 32, 8)

VARIANCE_WXH_NEON_DOTPROD(16, 4, 6)
VARIANCE_WXH_NEON_DOTPROD(16, 8, 7)
VARIANCE_WXH_NEON_DOTPROD(16, 16, 8)
VARIANCE_WXH_NEON_DOTPROD(16, 32, 9)
VARIANCE_WXH_NEON_DOTPROD(16, 64, 10)

VARIANCE_WXH_NEON_DOTPROD(32, 8, 8)
VARIANCE_WXH_NEON_DOTPROD(32, 16, 9)
VARIANCE_WXH_NEON_DOTPROD(32, 32, 10)
VARIANCE_WXH_NEON_DOTPROD(32, 64, 11)

VARIANCE_WXH_NEON_DOTPROD(64, 16, 10)
VARIANCE_WXH_NEON_DOTPROD(64, 32, 11)
VARIANCE_WXH_NEON_DOTPROD(64, 64, 12)
VARIANCE_WXH_NEON_DOTPROD(64, 128, 13)

VARIANCE_WXH_NEON_DOTPROD(128, 64, 13)
VARIANCE_WXH_NEON_DOTPROD(128, 128, 14)

#undef VARIANCE_WXH_NEON_DOTPROD
