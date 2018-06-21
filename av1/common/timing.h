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

#ifndef AOM_TIMING_H_
#define AOM_TIMING_H_

#include "aom/aom_integer.h"

#define MAX_NUM_OP_POINTS 32

typedef struct aom_timing {
  uint32_t num_units_in_display_tick;
  uint32_t time_scale;
  int equal_picture_interval;
  uint32_t num_ticks_per_picture;
} aom_timing_info_t;

typedef struct aom_dec_model_info {
  uint32_t num_units_in_decoding_tick;
  int encoder_decoder_buffer_delay_length;
  int buffer_removal_delay_length;
  int frame_presentation_delay_length;
} aom_dec_model_info_t;

typedef struct aom_dec_model_op_parameters {
  int decoder_model_param_present_flag;
  int64_t bitrate;
  int64_t buffer_size;
  int decoder_buffer_delay;
  int encoder_buffer_delay;
  int low_delay_mode_flag;
  int display_model_param_present_flag;
  int initial_display_delay;
} aom_dec_model_op_parameters_t;

typedef struct aom_op_timing_info_t {
  int64_t buffer_removal_delay;
} aom_op_timing_info_t;

void set_aom_dec_model_info(aom_dec_model_info_t *decoder_model);

void set_dec_model_op_parameters(aom_dec_model_op_parameters_t *op_params,
                                 int64_t bitrate);

#endif  // AOM_TIMING_H_
