/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_GLOBAL_MOTION_H_
#define AOM_AV1_ENCODER_GLOBAL_MOTION_H_

#include "aom/aom_integer.h"
#include "aom_scale/yv12config.h"
#include "av1/common/mv.h"
#include "av1/common/warped_motion.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CORNERS 4096
#define RANSAC_NUM_MOTIONS 1
#define GM_REFINEMENT_COUNT 5
#define MAX_DIRECTIONS 2

typedef enum {
  GLOBAL_MOTION_FEATURE_BASED,
  GLOBAL_MOTION_DISFLOW_BASED,
} GlobalMotionEstimationType;

unsigned char *av1_downconvert_frame(YV12_BUFFER_CONFIG *frm, int bit_depth);

typedef struct {
  double params[MAX_PARAMDIM - 1];
  int *inliers;
  int num_inliers;
} MotionModel;

// The structure holds a valid reference frame type and its temporal distance
// from the source frame.
typedef struct {
  int distance;
  MV_REFERENCE_FRAME frame;
} FrameDistPair;

void av1_convert_model_to_params(const double *params,
                                 WarpedMotionParams *model);

// TODO(sarahparker) These need to be retuned for speed 0 and 1 to
// maximize gains from segmented error metric
static const double erroradv_tr[] = { 0.65, 0.60, 0.65 };
static const double erroradv_prod_tr[] = { 20000, 18000, 16000 };

int av1_is_enough_erroradvantage(double best_erroradvantage, int params_cost,
                                 int erroradv_type);

void av1_compute_feature_segmentation_map(uint8_t *segment_map, int width,
                                          int height, int *inliers,
                                          int num_inliers);

// Returns the error between the result of applying motion 'wm' to the frame
// described by 'ref' and the frame described by 'dst'.
int64_t av1_warp_error(WarpedMotionParams *wm, int use_hbd, int bd,
                       const uint8_t *ref, int width, int height, int stride,
                       uint8_t *dst, int p_col, int p_row, int p_width,
                       int p_height, int p_stride, int subsampling_x,
                       int subsampling_y, int64_t best_error,
                       uint8_t *segment_map, int segment_map_stride);

// Returns the av1_warp_error between "dst" and the result of applying the
// motion params that result from fine-tuning "wm" to "ref". Note that "wm" is
// modified in place.
int64_t av1_refine_integerized_param(
    WarpedMotionParams *wm, TransformationType wmtype, int use_hbd, int bd,
    uint8_t *ref, int r_width, int r_height, int r_stride, uint8_t *dst,
    int d_width, int d_height, int d_stride, int n_refinements,
    int64_t best_frame_error, uint8_t *segment_map, int segment_map_stride,
    int64_t erroradv_threshold);

void av1_compute_global_motion(struct AV1_COMP *cpi);
#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AOM_AV1_ENCODER_GLOBAL_MOTION_H_
