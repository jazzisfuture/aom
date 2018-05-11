/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <arm_neon.h>
#include <aom_ports/mem.h>

#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/mem_neon.h"
#include "av1/common/arm/transpose_neon.h"

static INLINE int16x4_t convolve_s16_8_4(const int16x4_t s0, const int16x4_t s1,
                                         const int16x4_t s2, const int16x4_t s3,
                                         const int16x4_t s4, const int16x4_t s5,
                                         const int16x4_t s6, const int16x4_t s7,
                                         const int16_t *filter) {
  int16x4_t sum;

  sum = vmul_n_s16(s0, filter[0]);
  sum = vmla_n_s16(sum, s1, filter[1]);
  sum = vmla_n_s16(sum, s2, filter[2]);
  sum = vmla_n_s16(sum, s5, filter[5]);
  sum = vmla_n_s16(sum, s6, filter[6]);
  sum = vmla_n_s16(sum, s7, filter[7]);
  sum = vqadd_s16(sum, vmul_n_s16(s3, filter[3]));
  sum = vqadd_s16(sum, vmul_n_s16(s4, filter[4]));

  return sum;
}

static INLINE uint8x8_t convolve8_8(const int16x8_t s0, const int16x8_t s1,
                                    const int16x8_t s2, const int16x8_t s3,
                                    const int16x8_t s4, const int16x8_t s5,
                                    const int16x8_t s6, const int16x8_t s7,
                                    const int16_t *filter,
                                    const int16x8_t shift_round_0,
                                    const int16x8_t shift_by_bits) {
  int16x8_t sum;

  sum = vmulq_n_s16(s0, filter[0]);
  sum = vmlaq_n_s16(sum, s1, filter[1]);
  sum = vmlaq_n_s16(sum, s2, filter[2]);
  sum = vmlaq_n_s16(sum, s5, filter[5]);
  sum = vmlaq_n_s16(sum, s6, filter[6]);
  sum = vmlaq_n_s16(sum, s7, filter[7]);
  sum = vqaddq_s16(sum, vmulq_n_s16(s3, filter[3]));
  sum = vqaddq_s16(sum, vmulq_n_s16(s4, filter[4]));

  sum = vqrshlq_s16(sum, shift_round_0);
  sum = vqrshlq_s16(sum, shift_by_bits);

  return vqmovun_s16(sum);
}

void av1_convolve_x_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            InterpFilterParams *filter_params_x,
                            InterpFilterParams *filter_params_y,
                            const int subpel_x_q4, const int subpel_y_q4,
                            ConvolveParams *conv_params) {
  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t bits = FILTER_BITS - conv_params->round_0;

  (void)subpel_y_q4;
  (void)conv_params;
  (void)filter_params_y;

  uint8x8_t t0, t1, t2, t3;

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));

  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      *filter_params_x, subpel_x_q4 & SUBPEL_MASK);

  const int16_t shift_offset = -conv_params->round_0;
  int16x8_t shift_round_0 = vdupq_n_s16(shift_offset);

  const int16_t shift_bits = -bits;
  int16x8_t shift_by_bits = vdupq_n_s16(shift_bits);

  src -= horiz_offset;

  if (h == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t tt0, tt1, tt2, tt3, d01_temp, d23_temp;

    __builtin_prefetch(src + 0 * src_stride);
    __builtin_prefetch(src + 1 * src_stride);
    __builtin_prefetch(src + 2 * src_stride);
    __builtin_prefetch(src + 3 * src_stride);

    load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);
    tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
    tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
    tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
    s0 = vget_low_s16(tt0);
    s1 = vget_low_s16(tt1);
    s2 = vget_low_s16(tt2);
    s3 = vget_low_s16(tt3);
    s4 = vget_high_s16(tt0);
    s5 = vget_high_s16(tt1);
    s6 = vget_high_s16(tt2);
    __builtin_prefetch(dst + 0 * dst_stride);
    __builtin_prefetch(dst + 1 * dst_stride);
    __builtin_prefetch(dst + 2 * dst_stride);
    __builtin_prefetch(dst + 3 * dst_stride);
    src += 7;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      tt1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      tt2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      tt3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s7 = vget_low_s16(tt0);
      s8 = vget_low_s16(tt1);
      s9 = vget_low_s16(tt2);
      s10 = vget_low_s16(tt3);

      d0 = convolve_s16_8_4(s0, s1, s2, s3, s4, s5, s6, s7, x_filter);

      d1 = convolve_s16_8_4(s1, s2, s3, s4, s5, s6, s7, s8, x_filter);

      d2 = convolve_s16_8_4(s2, s3, s4, s5, s6, s7, s8, s9, x_filter);

      d3 = convolve_s16_8_4(s3, s4, s5, s6, s7, s8, s9, s10, x_filter);

      d01_temp = vqrshlq_s16(vcombine_s16(d0, d1), shift_round_0);
      d23_temp = vqrshlq_s16(vcombine_s16(d2, d3), shift_round_0);

      d01_temp = vqrshlq_s16(d01_temp, shift_by_bits);
      d23_temp = vqrshlq_s16(d23_temp, shift_by_bits);

      d01 = vqmovun_s16(d01_temp);
      d23 = vqmovun_s16(d23_temp);

      transpose_u8_4x4(&d01, &d23);

      if (w == 2) {
        vst1_lane_u16((uint16_t *)(dst + 0 * dst_stride),
                      vreinterpret_u16_u8(d01), 0);
        vst1_lane_u16((uint16_t *)(dst + 1 * dst_stride),
                      vreinterpret_u16_u8(d23), 0);
        vst1_lane_u16((uint16_t *)(dst + 2 * dst_stride),
                      vreinterpret_u16_u8(d01), 2);
        vst1_lane_u16((uint16_t *)(dst + 3 * dst_stride),
                      vreinterpret_u16_u8(d23), 2);
      } else {
        vst1_lane_u32((uint32_t *)(dst + 0 * dst_stride),
                      vreinterpret_u32_u8(d01), 0);
        vst1_lane_u32((uint32_t *)(dst + 1 * dst_stride),
                      vreinterpret_u32_u8(d23), 0);
        vst1_lane_u32((uint32_t *)(dst + 2 * dst_stride),
                      vreinterpret_u32_u8(d01), 1);
        vst1_lane_u32((uint32_t *)(dst + 3 * dst_stride),
                      vreinterpret_u32_u8(d23), 1);
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src += 4;
      dst += 4;
      w -= 4;
    } while (w > 0);
  } else {
    int width;
    const uint8_t *s;
    uint8x8_t t4, t5, t6, t7;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

    if (w <= 4) {
      do {
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        load_u8_8x8(src + 7, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                    &t7);
        src += 8 * src_stride;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        transpose_u8_4x8(&t0, &t1, &t2, &t3, t4, t5, t6, t7);

        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                         shift_round_0, shift_by_bits);
        t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                         shift_round_0, shift_by_bits);
        t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                         shift_round_0, shift_by_bits);
        t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                         shift_round_0, shift_by_bits);

        transpose_u8_8x4(&t0, &t1, &t2, &t3);

        if ((w == 4) && (h > 4)) {
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0), 0);
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1), 0);
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t2), 0);
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t3), 0);
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0), 1);
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1), 1);
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t2), 1);
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t3), 1);
          dst += dst_stride;
        } else if ((w == 4) && (h == 2)) {
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0), 0);
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1), 0);
          dst += dst_stride;
        } else if ((w == 2) && (h > 4)) {
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0), 0);
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1), 0);
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t2), 0);
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t3), 0);
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0), 2);
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1), 2);
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t2), 2);
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t3), 2);
          dst += dst_stride;
        } else if ((w == 2) && (h == 2)) {
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0), 0);
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1), 0);
          dst += dst_stride;
        }

        h -= 8;
      } while (h > 0);
    } else {
      uint8_t *d;
      int16x8_t s11, s12, s13, s14;

      do {
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        width = w;
        s = src + 7;
        d = dst;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        do {
          load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
          s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
          s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
          s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
          s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
          s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
          s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
          s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

          t0 = convolve8_8(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                           shift_round_0, shift_by_bits);

          t1 = convolve8_8(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                           shift_round_0, shift_by_bits);

          t2 = convolve8_8(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                           shift_round_0, shift_by_bits);

          t3 = convolve8_8(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                           shift_round_0, shift_by_bits);

          t4 = convolve8_8(s4, s5, s6, s7, s8, s9, s10, s11, x_filter,
                           shift_round_0, shift_by_bits);

          t5 = convolve8_8(s5, s6, s7, s8, s9, s10, s11, s12, x_filter,
                           shift_round_0, shift_by_bits);

          t6 = convolve8_8(s6, s7, s8, s9, s10, s11, s12, s13, x_filter,
                           shift_round_0, shift_by_bits);

          t7 = convolve8_8(s7, s8, s9, s10, s11, s12, s13, s14, x_filter,
                           shift_round_0, shift_by_bits);

          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          if (h != 2) {
            store_u8_8x8(d, dst_stride, t0, t1, t2, t3, t4, t5, t6, t7);
          } else {
            store_row2_u8_8x8(d, dst_stride, t0, t1);
          }
          s0 = s8;
          s1 = s9;
          s2 = s10;
          s3 = s11;
          s4 = s12;
          s5 = s13;
          s6 = s14;
          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src += 8 * src_stride;
        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
    }
  }
}
