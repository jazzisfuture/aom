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

#ifndef AOM_DSP_X86_SUM_SQUARES_SSE2_H_
#define AOM_DSP_X86_SUM_SQUARES_SSE2_H_

uint64_t aom_sum_squares_2d_i16_nxn_sse2(const int16_t *src, int stride,
                                         int width, int height, double *var);

uint64_t aom_sum_squares_2d_i16_4xn_sse2(const int16_t *src, int stride,
                                         int height, double *var);
uint64_t aom_sum_squares_2d_i16_4x4_sse2(const int16_t *src, int stride,
                                         double *var);

#endif  // AOM_DSP_X86_SUM_SQUARES_SSE2_H_
