/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_MOTION_SEARCH_H_
#define AOM_AV1_ENCODER_MOTION_SEARCH_H_

#include "av1/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                              BLOCK_SIZE bsize, int ref_idx, int *rate_mv);

void av1_joint_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                             BLOCK_SIZE bsize, int_mv *cur_mv,
                             const uint8_t *mask, int mask_stride,
                             int *rate_mv);

int av1_interinter_compound_motion_search(const AV1_COMP *const cpi,
                                          MACROBLOCK *x,
                                          const int_mv *const cur_mv,
                                          const BLOCK_SIZE bsize,
                                          const PREDICTION_MODE this_mode);

void av1_compound_single_motion_search_interinter(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int_mv *cur_mv,
    const uint8_t *mask, int mask_stride, int *rate_mv, int ref_idx);

void av1_compound_single_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                       BLOCK_SIZE bsize, MV *this_mv,
                                       const uint8_t *second_pred,
                                       const uint8_t *mask, int mask_stride,
                                       int *rate_mv, int ref_idx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_MOTION_SEARCH_H_
