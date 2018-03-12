/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tmmintrin.h>

#include "./av1_rtcd.h"

#include "av1/common/cfl.h"

#include "av1/common/x86/cfl_simd.h"

/**
 * Adds 4 pixels (in a 2x2 grid) and multiplies them by 2. Resulting in a more
 * precise version of a box filter 4:2:0 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
static INLINE void cfl_luma_subsampling_420_lbd_ssse3(const uint8_t *input,
                                                      int input_stride,
                                                      int16_t *pred_buf_q3,
                                                      int width, int height) {
  const __m128i twos = _mm_set1_epi8(2);
  const int16_t *end = pred_buf_q3 + (height >> 1) * CFL_BUF_LINE;
  const int luma_stride = input_stride << 1;

  __m128i top, bot, next_top, next_bot, top_16x8, bot_16x8, next_top_16x8,
      next_bot_16x8, sum_16x8, next_sum_16x8;
  do {
    if (width == 4) {
      top = _mm_cvtsi32_si128(*((int *)input));
      bot = _mm_cvtsi32_si128(*((int *)(input + input_stride)));
    } else if (width == 8) {
      top = _mm_loadl_epi64((__m128i *)input);
      bot = _mm_loadl_epi64((__m128i *)(input + input_stride));
    } else {
      top = _mm_loadu_si128((__m128i *)input);
      bot = _mm_loadu_si128((__m128i *)(input + input_stride));
      if (width == 32) {
        next_top = _mm_loadu_si128((__m128i *)(input + 16));
        next_bot = _mm_loadu_si128((__m128i *)(input + 16 + input_stride));
      }
    }

    top_16x8 = _mm_maddubs_epi16(top, twos);
    bot_16x8 = _mm_maddubs_epi16(bot, twos);
    sum_16x8 = _mm_add_epi16(top_16x8, bot_16x8);
    if (width == 32) {
      next_top_16x8 = _mm_maddubs_epi16(next_top, twos);
      next_bot_16x8 = _mm_maddubs_epi16(next_bot, twos);
      next_sum_16x8 = _mm_add_epi16(next_top_16x8, next_bot_16x8);
    }

    if (width == 4) {
      *((int *)pred_buf_q3) = _mm_cvtsi128_si32(sum_16x8);
    } else if (width == 8) {
      _mm_storel_epi64((__m128i *)pred_buf_q3, sum_16x8);
    } else {
      _mm_storeu_si128((__m128i *)pred_buf_q3, sum_16x8);
      if (width == 32) {
        _mm_storeu_si128((__m128i *)(pred_buf_q3 + 8), next_sum_16x8);
      }
    }

    input += luma_stride;
    pred_buf_q3 += CFL_BUF_LINE;
  } while (pred_buf_q3 < end);
}

CFL_GET_SUBSAMPLE_FUNCTION(ssse3)

static INLINE __m128i predict_unclipped(const __m128i *input, __m128i alpha_q12,
                                        __m128i dc_q0) {
  __m128i ac_q3 = _mm_loadu_si128(input);
  __m128i scaled_luma_q0 = _mm_mulhrs_epi16(ac_q3, alpha_q12);
  return _mm_add_epi16(scaled_luma_q0, dc_q0);
}

static INLINE void cfl_predict_lbd_x(const int16_t *pred_buf_q3, uint8_t *dst,
                                     int dst_stride, TX_SIZE tx_size,
                                     int alpha_q3, int width) {
  uint8_t *row_end = dst + tx_size_high[tx_size] * dst_stride;
  const __m128i alpha_q12 = _mm_set1_epi16(alpha_q3 << 9);
  const __m128i dc_q0 = _mm_set1_epi16(*dst);
  do {
    __m128i res = predict_unclipped((__m128i *)(pred_buf_q3), alpha_q12, dc_q0);
    if (width < 16) {
      res = _mm_packus_epi16(res, res);
      if (width == 4)
        *(uint32_t *)dst = _mm_cvtsi128_si32(res);
      else
        _mm_storel_epi64((__m128i *)dst, res);
    } else {
      __m128i next =
          predict_unclipped((__m128i *)(pred_buf_q3 + 8), alpha_q12, dc_q0);
      res = _mm_packus_epi16(res, next);
      _mm_storeu_si128((__m128i *)dst, res);
      if (width == 32) {
        res =
            predict_unclipped((__m128i *)(pred_buf_q3 + 16), alpha_q12, dc_q0);
        next =
            predict_unclipped((__m128i *)(pred_buf_q3 + 24), alpha_q12, dc_q0);
        res = _mm_packus_epi16(res, next);
        _mm_storeu_si128((__m128i *)(dst + 16), res);
      }
    }
    dst += dst_stride;
    pred_buf_q3 += CFL_BUF_LINE;
  } while (dst < row_end);
}

static INLINE __m128i highbd_max_epi16(int bd) {
  const __m128i neg_one = _mm_set1_epi16(-1);
  // (1 << bd) - 1 => -(-1 << bd) -1 => -1 - (-1 << bd) => -1 ^ (-1 << bd)
  return _mm_xor_si128(_mm_slli_epi16(neg_one, bd), neg_one);
}

static INLINE __m128i highbd_clamp_epi16(__m128i u, __m128i zero, __m128i max) {
  return _mm_max_epi16(_mm_min_epi16(u, max), zero);
}

static INLINE void cfl_predict_hbd(__m128i *dst, __m128i *src,
                                   __m128i alpha_q12, __m128i dc_q0,
                                   __m128i max) {
  __m128i res = predict_unclipped(src, alpha_q12, dc_q0);
  _mm_storeu_si128(dst, highbd_clamp_epi16(res, _mm_setzero_si128(), max));
}

static INLINE void cfl_predict_hbd_x(const int16_t *pred_buf_q3, uint16_t *dst,
                                     int dst_stride, TX_SIZE tx_size,
                                     int alpha_q3, int bd, int width) {
  uint16_t *row_end = dst + tx_size_high[tx_size] * dst_stride;
  const __m128i alpha_q12 = _mm_set1_epi16(alpha_q3 << 9);
  const __m128i dc_q0 = _mm_set1_epi16(*dst);
  const __m128i max = highbd_max_epi16(bd);
  do {
    if (width == 4) {
      __m128i res =
          predict_unclipped((__m128i *)(pred_buf_q3), alpha_q12, dc_q0);
      _mm_storel_epi64((__m128i *)dst,
                       highbd_clamp_epi16(res, _mm_setzero_si128(), max));
    } else {
      cfl_predict_hbd((__m128i *)dst, (__m128i *)pred_buf_q3, alpha_q12, dc_q0,
                      max);
    }
    if (width >= 16)
      cfl_predict_hbd((__m128i *)(dst + 8), (__m128i *)(pred_buf_q3 + 8),
                      alpha_q12, dc_q0, max);
    if (width == 32) {
      cfl_predict_hbd((__m128i *)(dst + 16), (__m128i *)(pred_buf_q3 + 16),
                      alpha_q12, dc_q0, max);
      cfl_predict_hbd((__m128i *)(dst + 24), (__m128i *)(pred_buf_q3 + 24),
                      alpha_q12, dc_q0, max);
    }
    dst += dst_stride;
    pred_buf_q3 += CFL_BUF_LINE;
  } while (dst < row_end);
}

CFL_PREDICT_LBD_X(4, ssse3)
CFL_PREDICT_LBD_X(8, ssse3)
CFL_PREDICT_LBD_X(16, ssse3)
CFL_PREDICT_LBD_X(32, ssse3)

CFL_PREDICT_HBD_X(4, ssse3)
CFL_PREDICT_HBD_X(8, ssse3)
CFL_PREDICT_HBD_X(16, ssse3)
CFL_PREDICT_HBD_X(32, ssse3)

cfl_predict_lbd_fn get_predict_lbd_fn_ssse3(TX_SIZE tx_size) {
  static const cfl_predict_lbd_fn predict_lbd[4] = { cfl_predict_lbd_4_ssse3,
                                                     cfl_predict_lbd_8_ssse3,
                                                     cfl_predict_lbd_16_ssse3,
                                                     cfl_predict_lbd_32_ssse3 };
  return predict_lbd[(tx_size_wide_log2[tx_size] - tx_size_wide_log2[0]) & 3];
}

cfl_predict_hbd_fn get_predict_hbd_fn_ssse3(TX_SIZE tx_size) {
  static const cfl_predict_hbd_fn predict_hbd[4] = { cfl_predict_hbd_4_ssse3,
                                                     cfl_predict_hbd_8_ssse3,
                                                     cfl_predict_hbd_16_ssse3,
                                                     cfl_predict_hbd_32_ssse3 };
  return predict_hbd[(tx_size_wide_log2[tx_size] - tx_size_wide_log2[0]) & 3];
}
