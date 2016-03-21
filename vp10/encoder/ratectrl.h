/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_ENCODER_RATECTRL_H_
#define VP10_ENCODER_RATECTRL_H_

#include "aom/vpx_codec.h"
#include "aom/vpx_integer.h"

#include "vp10/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bits Per MB at different Q (Multiplied by 512)
#define BPER_MB_NORMBITS 9

#define MIN_GF_INTERVAL 4
#define MAX_GF_INTERVAL 16
#define FIXED_GF_INTERVAL 8  // Used in some testing modes only

typedef enum {
  INTER_NORMAL = 0,
  INTER_HIGH = 1,
  GF_ARF_LOW = 2,
  GF_ARF_STD = 3,
  KF_STD = 4,
  RATE_FACTOR_LEVELS = 5
} RATE_FACTOR_LEVEL;

// Internal frame scaling level.
typedef enum {
  UNSCALED = 0,     // Frame is unscaled.
  SCALE_STEP1 = 1,  // First-level down-scaling.
  FRAME_SCALE_STEPS
} FRAME_SCALE_LEVEL;

// Frame dimensions multiplier wrt the native frame size, in 1/16ths,
// specified for the scale-up case.
// e.g. 24 => 16/24 = 2/3 of native size. The restriction to 1/16th is
// intended to match the capabilities of the normative scaling filters,
// giving precedence to the up-scaling accuracy.
static const int frame_scale_factor[FRAME_SCALE_STEPS] = { 16, 24 };

// Multiplier of the target rate to be used as threshold for triggering scaling.
static const double rate_thresh_mult[FRAME_SCALE_STEPS] = { 1.0, 2.0 };

// Scale dependent Rate Correction Factor multipliers. Compensates for the
// greater number of bits per pixel generated in down-scaled frames.
static const double rcf_mult[FRAME_SCALE_STEPS] = { 1.0, 2.0 };

typedef struct {
  // Rate targetting variables
  int base_frame_target;  // A baseline frame target before adjustment
                          // for previous under or over shoot.
  int this_frame_target;  // Actual frame target after rc adjustment.
  int projected_frame_size;
  int sb64_target_rate;
  int last_q[FRAME_TYPES];  // Separate values for Intra/Inter
  int last_boosted_qindex;  // Last boosted GF/KF/ARF q
  int last_kf_qindex;       // Q index of the last key frame coded.

  int gfu_boost;
  int last_boost;
  int kf_boost;

  double rate_correction_factors[RATE_FACTOR_LEVELS];

  int frames_since_golden;
  int frames_till_gf_update_due;
  int min_gf_interval;
  int max_gf_interval;
  int static_scene_max_gf_interval;
  int baseline_gf_interval;
  int constrained_gf_group;
  int frames_to_key;
  int frames_since_key;
  int this_key_frame_forced;
  int next_key_frame_forced;
  int source_alt_ref_pending;
  int source_alt_ref_active;
  int is_src_frame_alt_ref;

  int avg_frame_bandwidth;  // Average frame size target for clip
  int min_frame_bandwidth;  // Minimum allocation used for any frame
  int max_frame_bandwidth;  // Maximum burst rate allowed for a frame.

  int ni_av_qi;
  int ni_tot_qi;
  int ni_frames;
  int avg_frame_qindex[FRAME_TYPES];
  double tot_q;
  double avg_q;

  int64_t buffer_level;
  int64_t bits_off_target;
  int64_t vbr_bits_off_target;
  int64_t vbr_bits_off_target_fast;

  int decimation_factor;
  int decimation_count;

  int rolling_target_bits;
  int rolling_actual_bits;

  int long_rolling_target_bits;
  int long_rolling_actual_bits;

  int rate_error_estimate;

  int64_t total_actual_bits;
  int64_t total_target_bits;
  int64_t total_target_vs_actual;

  int worst_quality;
  int best_quality;

  int64_t starting_buffer_level;
  int64_t optimal_buffer_level;
  int64_t maximum_buffer_size;

  // rate control history for last frame(1) and the frame before(2).
  // -1: undershot
  //  1: overshoot
  //  0: not initialized.
  int rc_1_frame;
  int rc_2_frame;
  int q_1_frame;
  int q_2_frame;

  // Auto frame-scaling variables.
  FRAME_SCALE_LEVEL frame_size_selector;
  FRAME_SCALE_LEVEL next_frame_size_selector;
  int frame_width[FRAME_SCALE_STEPS];
  int frame_height[FRAME_SCALE_STEPS];
  int rf_level_maxq[RATE_FACTOR_LEVELS];
} RATE_CONTROL;

struct VP10_COMP;
struct VP10EncoderConfig;

void vp10_rc_init(const struct VP10EncoderConfig *oxcf, int pass,
                  RATE_CONTROL *rc);

int vp10_estimate_bits_at_q(FRAME_TYPE frame_kind, int q, int mbs,
                            double correction_factor,
                            vpx_bit_depth_t bit_depth);

double vp10_convert_qindex_to_q(int qindex, vpx_bit_depth_t bit_depth);

void vp10_rc_init_minq_luts(void);

int vp10_rc_get_default_min_gf_interval(int width, int height,
                                        double framerate);
// Note vp10_rc_get_default_max_gf_interval() requires the min_gf_interval to
// be passed in to ensure that the max_gf_interval returned is at least as bis
// as that.
int vp10_rc_get_default_max_gf_interval(double framerate, int min_frame_rate);

// Generally at the high level, the following flow is expected
// to be enforced for rate control:
// First call per frame, one of:
//   vp10_rc_get_one_pass_vbr_params()
//   vp10_rc_get_one_pass_cbr_params()
//   vp10_rc_get_first_pass_params()
//   vp10_rc_get_second_pass_params()
// depending on the usage to set the rate control encode parameters desired.
//
// Then, call encode_frame_to_data_rate() to perform the
// actual encode. This function will in turn call encode_frame()
// one or more times, followed by one of:
//   vp10_rc_postencode_update()
//   vp10_rc_postencode_update_drop_frame()
//
// The majority of rate control parameters are only expected
// to be set in the vp10_rc_get_..._params() functions and
// updated during the vp10_rc_postencode_update...() functions.
// The only exceptions are vp10_rc_drop_frame() and
// vp10_rc_update_rate_correction_factors() functions.

// Functions to set parameters for encoding before the actual
// encode_frame_to_data_rate() function.
void vp10_rc_get_one_pass_vbr_params(struct VP10_COMP *cpi);
void vp10_rc_get_one_pass_cbr_params(struct VP10_COMP *cpi);

// Post encode update of the rate control parameters based
// on bytes used
void vp10_rc_postencode_update(struct VP10_COMP *cpi, uint64_t bytes_used);
// Post encode update of the rate control parameters for dropped frames
void vp10_rc_postencode_update_drop_frame(struct VP10_COMP *cpi);

// Updates rate correction factors
// Changes only the rate correction factors in the rate control structure.
void vp10_rc_update_rate_correction_factors(struct VP10_COMP *cpi);

// Decide if we should drop this frame: For 1-pass CBR.
// Changes only the decimation count in the rate control structure
int vp10_rc_drop_frame(struct VP10_COMP *cpi);

// Computes frame size bounds.
void vp10_rc_compute_frame_size_bounds(const struct VP10_COMP *cpi,
                                       int this_frame_target,
                                       int *frame_under_shoot_limit,
                                       int *frame_over_shoot_limit);

// Picks q and q bounds given the target for bits
int vp10_rc_pick_q_and_bounds(const struct VP10_COMP *cpi, int *bottom_index,
                              int *top_index);

// Estimates q to achieve a target bits per frame
int vp10_rc_regulate_q(const struct VP10_COMP *cpi, int target_bits_per_frame,
                       int active_best_quality, int active_worst_quality);

// Estimates bits per mb for a given qindex and correction factor.
int vp10_rc_bits_per_mb(FRAME_TYPE frame_type, int qindex,
                        double correction_factor, vpx_bit_depth_t bit_depth);

// Clamping utilities for bitrate targets for iframes and pframes.
int vp10_rc_clamp_iframe_target_size(const struct VP10_COMP *const cpi,
                                     int target);
int vp10_rc_clamp_pframe_target_size(const struct VP10_COMP *const cpi,
                                     int target);
// Utility to set frame_target into the RATE_CONTROL structure
// This function is called only from the vp10_rc_get_..._params() functions.
void vp10_rc_set_frame_target(struct VP10_COMP *cpi, int target);

// Computes a q delta (in "q index" terms) to get from a starting q value
// to a target q value
int vp10_compute_qdelta(const RATE_CONTROL *rc, double qstart, double qtarget,
                        vpx_bit_depth_t bit_depth);

// Computes a q delta (in "q index" terms) to get from a starting q value
// to a value that should equate to the given rate ratio.
int vp10_compute_qdelta_by_rate(const RATE_CONTROL *rc, FRAME_TYPE frame_type,
                                int qindex, double rate_target_ratio,
                                vpx_bit_depth_t bit_depth);

int vp10_frame_type_qdelta(const struct VP10_COMP *cpi, int rf_level, int q);

void vp10_rc_update_framerate(struct VP10_COMP *cpi);

void vp10_rc_set_gf_interval_range(const struct VP10_COMP *const cpi,
                                   RATE_CONTROL *const rc);

void vp10_set_target_rate(struct VP10_COMP *cpi);

int vp10_resize_one_pass_cbr(struct VP10_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_ENCODER_RATECTRL_H_
