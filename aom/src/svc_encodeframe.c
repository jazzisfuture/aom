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

/**
 * @file
 * AV1 SVC encoding support via libaom
 */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define AOM_DISABLE_CTRL_TYPECHECKS 1
#include "./aom_config.h"
#include "aom/aom_encoder.h"
#include "aom/aomcx.h"
#include "aom/svc_context.h"
#include "aom_mem/aom_mem.h"
#include "av1/common/onyxc_int.h"

#ifdef __MINGW32__
#define strtok_r strtok_s
#ifndef MINGW_HAS_SECURE_API
// proto from /usr/x86_64-w64-mingw32/include/sec_api/string_s.h
_CRTIMP char *__cdecl strtok_s(char *str, const char *delim, char **context);
#endif /* MINGW_HAS_SECURE_API */
#endif /* __MINGW32__ */

#ifdef _MSC_VER
#define strdup _strdup
#define strtok_r strtok_s
#endif

#define SVC_REFERENCE_FRAMES 8
#define SUPERFRAME_SLOTS (8)
#define SUPERFRAME_BUFFER_SIZE (SUPERFRAME_SLOTS * sizeof(uint32_t) + 2)

#define MAX_QUANTIZER 63

static const int DEFAULT_SCALE_FACTORS_NUM[AOM_SS_MAX_LAYERS] = {4, 5, 7, 11,
                                                                 16};

static const int DEFAULT_SCALE_FACTORS_DEN[AOM_SS_MAX_LAYERS] = {16, 16, 16, 16,
                                                                 16};

typedef enum {
  QUANTIZER = 0,
  BITRATE,
  SCALE_FACTOR,
  AUTO_ALT_REF,
  ALL_OPTION_TYPES
} LAYER_OPTION_TYPE;

static const int option_max_values[ALL_OPTION_TYPES] = {63, INT_MAX, INT_MAX,
                                                        1};

static const int option_min_values[ALL_OPTION_TYPES] = {0, 0, 1, 0};

// One encoded frame
typedef struct FrameData {
  void *buf;                     // compressed data buffer
  size_t size;                   // length of compressed data
  aom_codec_frame_flags_t flags; /**< flags for this frame */
  struct FrameData *next;
} FrameData;

static SvcInternal_t *get_svc_internal(SvcContext *svc_ctx) {
  if (svc_ctx == NULL) return NULL;
  if (svc_ctx->internal == NULL) {
    SvcInternal_t *const si = (SvcInternal_t *)malloc(sizeof(*si));
    if (si != NULL) {
      memset(si, 0, sizeof(*si));
    }
    svc_ctx->internal = si;
  }
  return (SvcInternal_t *)svc_ctx->internal;
}

static const SvcInternal_t *get_const_svc_internal(const SvcContext *svc_ctx) {
  if (svc_ctx == NULL) return NULL;
  return (const SvcInternal_t *)svc_ctx->internal;
}

static void svc_log_reset(SvcContext *svc_ctx) {
  SvcInternal_t *const si = (SvcInternal_t *)svc_ctx->internal;
  si->message_buffer[0] = '\0';
}

static int svc_log(SvcContext *svc_ctx, SVC_LOG_LEVEL level, const char *fmt,
                   ...) {
  char buf[512];
  int retval = 0;
  va_list ap;
  SvcInternal_t *const si = get_svc_internal(svc_ctx);

  if (level > svc_ctx->log_level) {
    return retval;
  }

  va_start(ap, fmt);
  retval = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (svc_ctx->log_print) {
    printf("%s", buf);
  } else {
    strncat(si->message_buffer, buf,
            sizeof(si->message_buffer) - strlen(si->message_buffer) - 1);
  }

  if (level == SVC_LOG_ERROR) {
    si->codec_ctx->err_detail = si->message_buffer;
  }
  return retval;
}

static aom_codec_err_t extract_option(LAYER_OPTION_TYPE type, char *input,
                                      int *value0, int *value1) {
  if (type == SCALE_FACTOR) {
    *value0 = strtol(input, &input, 10);
    if (*input++ != '/') return AOM_CODEC_INVALID_PARAM;
    *value1 = strtol(input, &input, 10);

    if (*value0 < option_min_values[SCALE_FACTOR] ||
        *value1 < option_min_values[SCALE_FACTOR] ||
        *value0 > option_max_values[SCALE_FACTOR] ||
        *value1 > option_max_values[SCALE_FACTOR] ||
        *value0 > *value1)  // num shouldn't be greater than den
      return AOM_CODEC_INVALID_PARAM;
  } else {
    *value0 = atoi(input);
    if (*value0 < option_min_values[type] || *value0 > option_max_values[type])
      return AOM_CODEC_INVALID_PARAM;
  }
  return AOM_CODEC_OK;
}

static aom_codec_err_t parse_layer_options_from_string(SvcContext *svc_ctx,
                                                       LAYER_OPTION_TYPE type,
                                                       const char *input,
                                                       int *option0,
                                                       int *option1) {
  int i;
  aom_codec_err_t res = AOM_CODEC_OK;
  char *input_string;
  char *token;
  const char *delim = ",";
  char *save_ptr;

  if (input == NULL || option0 == NULL ||
      (option1 == NULL && type == SCALE_FACTOR))
    return AOM_CODEC_INVALID_PARAM;

  input_string = strdup(input);
  token = strtok_r(input_string, delim, &save_ptr);
  for (i = 0; i < svc_ctx->spatial_layers; ++i) {
    if (token != NULL) {
      res = extract_option(type, token, option0 + i, option1 + i);
      if (res != AOM_CODEC_OK) break;
      token = strtok_r(NULL, delim, &save_ptr);
    } else {
      break;
    }
  }
  if (res == AOM_CODEC_OK && i != svc_ctx->spatial_layers) {
    svc_log(svc_ctx, SVC_LOG_ERROR,
            "svc: layer params type: %d    %d values required, "
            "but only %d specified\n",
            type, svc_ctx->spatial_layers, i);
    res = AOM_CODEC_INVALID_PARAM;
  }
  free(input_string);
  return res;
}

/**
 * Parse SVC encoding options
 * Format: encoding-mode=<svc_mode>,layers=<layer_count>
 *         scale-factors=<n1>/<d1>,<n2>/<d2>,...
 *         quantizers=<q1>,<q2>,...
 * svc_mode = [i|ip|alt_ip|gf]
 */
static aom_codec_err_t parse_options(SvcContext *svc_ctx, const char *options) {
  char *input_string;
  char *option_name;
  char *option_value;
  char *input_ptr;
  SvcInternal_t *const si = get_svc_internal(svc_ctx);
  aom_codec_err_t res = AOM_CODEC_OK;
  int i, alt_ref_enabled = 0;

  if (options == NULL) return AOM_CODEC_OK;
  input_string = strdup(options);

  // parse option name
  option_name = strtok_r(input_string, "=", &input_ptr);
  while (option_name != NULL) {
    // parse option value
    option_value = strtok_r(NULL, " ", &input_ptr);
    if (option_value == NULL) {
      svc_log(svc_ctx, SVC_LOG_ERROR, "option missing value: %s\n",
              option_name);
      res = AOM_CODEC_INVALID_PARAM;
      break;
    }
    if (strcmp("spatial-layers", option_name) == 0) {
      svc_ctx->spatial_layers = atoi(option_value);
    } else if (strcmp("temporal-layers", option_name) == 0) {
      svc_ctx->temporal_layers = atoi(option_value);
    } else if (strcmp("scale-factors", option_name) == 0) {
      res = parse_layer_options_from_string(svc_ctx, SCALE_FACTOR, option_value,
                                            si->svc_params.scaling_factor_num,
                                            si->svc_params.scaling_factor_den);
      if (res != AOM_CODEC_OK) break;
    } else if (strcmp("max-quantizers", option_name) == 0) {
      res =
          parse_layer_options_from_string(svc_ctx, QUANTIZER, option_value,
                                          si->svc_params.max_quantizers, NULL);
      if (res != AOM_CODEC_OK) break;
    } else if (strcmp("min-quantizers", option_name) == 0) {
      res =
          parse_layer_options_from_string(svc_ctx, QUANTIZER, option_value,
                                          si->svc_params.min_quantizers, NULL);
      if (res != AOM_CODEC_OK) break;
    } else if (strcmp("auto-alt-refs", option_name) == 0) {
      res = parse_layer_options_from_string(svc_ctx, AUTO_ALT_REF, option_value,
                                            si->enable_auto_alt_ref, NULL);
      if (res != AOM_CODEC_OK) break;
    } else if (strcmp("bitrates", option_name) == 0) {
      res = parse_layer_options_from_string(svc_ctx, BITRATE, option_value,
                                            si->bitrates, NULL);
      if (res != AOM_CODEC_OK) break;
    } else if (strcmp("multi-frame-contexts", option_name) == 0) {
      si->use_multiple_frame_contexts = atoi(option_value);
    } else {
      svc_log(svc_ctx, SVC_LOG_ERROR, "invalid option: %s\n", option_name);
      res = AOM_CODEC_INVALID_PARAM;
      break;
    }
    option_name = strtok_r(NULL, "=", &input_ptr);
  }
  free(input_string);

  for (i = 0; i < svc_ctx->spatial_layers; ++i) {
    if (si->svc_params.max_quantizers[i] > MAX_QUANTIZER ||
        si->svc_params.max_quantizers[i] < 0 ||
        si->svc_params.min_quantizers[i] > si->svc_params.max_quantizers[i] ||
        si->svc_params.min_quantizers[i] < 0)
      res = AOM_CODEC_INVALID_PARAM;
  }

  if (si->use_multiple_frame_contexts &&
      (svc_ctx->spatial_layers > 3 ||
       svc_ctx->spatial_layers * svc_ctx->temporal_layers > 4))
    res = AOM_CODEC_INVALID_PARAM;

  for (i = 0; i < svc_ctx->spatial_layers; ++i)
    alt_ref_enabled += si->enable_auto_alt_ref[i];
  if (alt_ref_enabled > REF_FRAMES - svc_ctx->spatial_layers) {
    svc_log(svc_ctx, SVC_LOG_ERROR,
            "svc: auto alt ref: Maxinum %d(REF_FRAMES - layers) layers could"
            "enabled auto alt reference frame, but % layers are enabled\n",
            REF_FRAMES - svc_ctx->spatial_layers, alt_ref_enabled);
    res = AOM_CODEC_INVALID_PARAM;
  }

  return res;
}

aom_codec_err_t aom_svc_set_options(SvcContext *svc_ctx, const char *options) {
  SvcInternal_t *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || options == NULL || si == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  strncpy(si->options, options, sizeof(si->options));
  si->options[sizeof(si->options) - 1] = '\0';
  return AOM_CODEC_OK;
}

void assign_layer_bitrates(const SvcContext *svc_ctx,
                           aom_codec_enc_cfg_t *const enc_cfg) {
  int i;
  const SvcInternal_t *const si = get_const_svc_internal(svc_ctx);
  int sl, tl, spatial_layer_target;

  if (svc_ctx->temporal_layering_mode != 0) {
    if (si->bitrates[0] != 0) {
      enc_cfg->rc_target_bitrate = 0;
      for (sl = 0; sl < svc_ctx->spatial_layers; ++sl) {
        enc_cfg->ss_target_bitrate[sl * svc_ctx->temporal_layers] = 0;
        for (tl = 0; tl < svc_ctx->temporal_layers; ++tl) {
          enc_cfg->ss_target_bitrate[sl * svc_ctx->temporal_layers] +=
              (unsigned int)si->bitrates[sl * svc_ctx->temporal_layers + tl];
          enc_cfg->layer_target_bitrate[sl * svc_ctx->temporal_layers + tl] =
              si->bitrates[sl * svc_ctx->temporal_layers + tl];
        }
      }
    } else {
      float total = 0;
      float alloc_ratio[AOM_MAX_LAYERS] = {0};

      for (sl = 0; sl < svc_ctx->spatial_layers; ++sl) {
        if (si->svc_params.scaling_factor_den[sl] > 0) {
          alloc_ratio[sl] =
              (float)(si->svc_params.scaling_factor_num[sl] * 1.0 /
                      si->svc_params.scaling_factor_den[sl]);
          total += alloc_ratio[sl];
        }
      }

      for (sl = 0; sl < svc_ctx->spatial_layers; ++sl) {
        enc_cfg->ss_target_bitrate[sl] = spatial_layer_target =
            (unsigned int)(enc_cfg->rc_target_bitrate * alloc_ratio[sl] /
                           total);
        if (svc_ctx->temporal_layering_mode == 3) {
          enc_cfg->layer_target_bitrate[sl * svc_ctx->temporal_layers] =
              spatial_layer_target >> 1;
          enc_cfg->layer_target_bitrate[sl * svc_ctx->temporal_layers + 1] =
              (spatial_layer_target >> 1) + (spatial_layer_target >> 2);
          enc_cfg->layer_target_bitrate[sl * svc_ctx->temporal_layers + 2] =
              spatial_layer_target;
        } else if (svc_ctx->temporal_layering_mode == 2 ||
                   svc_ctx->temporal_layering_mode == 1) {
          enc_cfg->layer_target_bitrate[sl * svc_ctx->temporal_layers] =
              spatial_layer_target * 2 / 3;
          enc_cfg->layer_target_bitrate[sl * svc_ctx->temporal_layers + 1] =
              spatial_layer_target;
        } else {
          // User should explicitly assign bitrates in this case.
          assert(0);
        }
      }
    }
  } else {
    if (si->bitrates[0] != 0) {
      enc_cfg->rc_target_bitrate = 0;
      for (i = 0; i < svc_ctx->spatial_layers; ++i) {
        enc_cfg->ss_target_bitrate[i] = (unsigned int)si->bitrates[i];
        enc_cfg->rc_target_bitrate += si->bitrates[i];
      }
    } else {
      float total = 0;
      float alloc_ratio[AOM_MAX_LAYERS] = {0};

      for (i = 0; i < svc_ctx->spatial_layers; ++i) {
        if (si->svc_params.scaling_factor_den[i] > 0) {
          alloc_ratio[i] = (float)(si->svc_params.scaling_factor_num[i] * 1.0 /
                                   si->svc_params.scaling_factor_den[i]);

          alloc_ratio[i] *= alloc_ratio[i];
          total += alloc_ratio[i];
        }
      }
      for (i = 0; i < AOM_SS_MAX_LAYERS; ++i) {
        if (total > 0) {
          enc_cfg->layer_target_bitrate[i] =
              (unsigned int)(enc_cfg->rc_target_bitrate * alloc_ratio[i] /
                             total);
        }
      }
    }
  }
}

aom_codec_err_t aom_svc_init(SvcContext *svc_ctx, aom_codec_ctx_t *codec_ctx,
                             aom_codec_iface_t *iface,
                             aom_codec_enc_cfg_t *enc_cfg) {
  aom_codec_err_t res;
  int i, sl, tl;
  SvcInternal_t *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || codec_ctx == NULL || iface == NULL ||
      enc_cfg == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }
  if (si == NULL) return AOM_CODEC_MEM_ERROR;

  si->codec_ctx = codec_ctx;

  si->width = enc_cfg->g_w;
  si->height = enc_cfg->g_h;

  if (enc_cfg->kf_max_dist < 2) {
    svc_log(svc_ctx, SVC_LOG_ERROR, "key frame distance too small: %d\n",
            enc_cfg->kf_max_dist);
    return AOM_CODEC_INVALID_PARAM;
  }
  si->kf_dist = enc_cfg->kf_max_dist;

  if (svc_ctx->spatial_layers == 0)
    svc_ctx->spatial_layers = AOM_SS_DEFAULT_LAYERS;
  if (svc_ctx->spatial_layers < 1 ||
      svc_ctx->spatial_layers > AOM_SS_MAX_LAYERS) {
    svc_log(svc_ctx, SVC_LOG_ERROR, "spatial layers: invalid value: %d\n",
            svc_ctx->spatial_layers);
    return AOM_CODEC_INVALID_PARAM;
  }

  // Note: temporal_layering_mode only applies to one-pass CBR
  // si->svc_params.temporal_layering_mode = svc_ctx->temporal_layering_mode;
  if (svc_ctx->temporal_layering_mode == 3) {
    svc_ctx->temporal_layers = 3;
  } else if (svc_ctx->temporal_layering_mode == 2 ||
             svc_ctx->temporal_layering_mode == 1) {
    svc_ctx->temporal_layers = 2;
  }

  for (sl = 0; sl < AOM_SS_MAX_LAYERS; ++sl) {
    si->svc_params.scaling_factor_num[sl] = DEFAULT_SCALE_FACTORS_NUM[sl];
    si->svc_params.scaling_factor_den[sl] = DEFAULT_SCALE_FACTORS_DEN[sl];
  }
  for (tl = 0; tl < svc_ctx->temporal_layers; ++tl) {
    for (sl = 0; sl < svc_ctx->spatial_layers; ++sl) {
      i = sl * svc_ctx->temporal_layers + tl;
      si->svc_params.max_quantizers[i] = MAX_QUANTIZER;
      si->svc_params.min_quantizers[i] = 0;
    }
  }

  // Parse aggregate command line options. Options must start with
  // "layers=xx" then followed by other options
  res = parse_options(svc_ctx, si->options);
  if (res != AOM_CODEC_OK) return res;

  if (svc_ctx->spatial_layers < 1) svc_ctx->spatial_layers = 1;
  if (svc_ctx->spatial_layers > AOM_SS_MAX_LAYERS)
    svc_ctx->spatial_layers = AOM_SS_MAX_LAYERS;

  if (svc_ctx->temporal_layers < 1) svc_ctx->temporal_layers = 1;
  if (svc_ctx->temporal_layers > AOM_TS_MAX_LAYERS)
    svc_ctx->temporal_layers = AOM_TS_MAX_LAYERS;

  if (svc_ctx->temporal_layers * svc_ctx->spatial_layers > AOM_MAX_LAYERS) {
    svc_log(svc_ctx, SVC_LOG_ERROR,
            "spatial layers * temporal layers exceeds the maximum number of "
            "allowed layers of %d\n",
            svc_ctx->spatial_layers * svc_ctx->temporal_layers,
            (int)AOM_MAX_LAYERS);
    return AOM_CODEC_INVALID_PARAM;
  }
  assign_layer_bitrates(svc_ctx, enc_cfg);

#if CONFIG_SPATIAL_SVC
  for (i = 0; i < svc_ctx->spatial_layers; ++i)
    enc_cfg->ss_enable_auto_alt_ref[i] = si->enable_auto_alt_ref[i];
#endif

  if (svc_ctx->temporal_layers > 1) {
    int i;
    for (i = 0; i < svc_ctx->temporal_layers; ++i) {
      enc_cfg->ts_target_bitrate[i] =
          enc_cfg->rc_target_bitrate / svc_ctx->temporal_layers;
      enc_cfg->ts_rate_decimator[i] = 1 << (svc_ctx->temporal_layers - 1 - i);
    }
  }

  if (svc_ctx->threads) enc_cfg->g_threads = svc_ctx->threads;

  // Modify encoder configuration
  enc_cfg->ss_number_layers = svc_ctx->spatial_layers;
  enc_cfg->ts_number_layers = svc_ctx->temporal_layers;

  if (enc_cfg->rc_end_usage == AOM_CBR) {
    enc_cfg->rc_resize_allowed = 0;
    enc_cfg->rc_min_quantizer = 2;
    enc_cfg->rc_max_quantizer = 56;
    enc_cfg->rc_undershoot_pct = 50;
    enc_cfg->rc_overshoot_pct = 50;
    enc_cfg->rc_buf_initial_sz = 500;
    enc_cfg->rc_buf_optimal_sz = 600;
    enc_cfg->rc_buf_sz = 1000;
    enc_cfg->rc_dropframe_thresh = 0;
  }

  if (enc_cfg->g_error_resilient == 0 && si->use_multiple_frame_contexts == 0)
    enc_cfg->g_error_resilient = 1;

  // Initialize codec
  res = aom_codec_enc_init(codec_ctx, iface, enc_cfg, AOM_CODEC_USE_PSNR);
  if (res != AOM_CODEC_OK) {
    svc_log(svc_ctx, SVC_LOG_ERROR, "svc_enc_init error\n");
    return res;
  }
  if (svc_ctx->spatial_layers > 1 || svc_ctx->temporal_layers > 1) {
    aom_codec_control(codec_ctx, AV1E_SET_SVC, 1);
    aom_codec_control(codec_ctx, AV1E_SET_SVC_PARAMETERS, &si->svc_params);
  }
  return AOM_CODEC_OK;
}

/**
 * Encode a frame into multiple layers
 * Create a superframe containing the individual layers
 */
aom_codec_err_t aom_svc_encode(SvcContext *svc_ctx, aom_codec_ctx_t *codec_ctx,
                               struct aom_image *rawimg, aom_codec_pts_t pts,
                               int64_t duration, int deadline) {
  aom_codec_err_t res;
  aom_codec_iter_t iter;
  const aom_codec_cx_pkt_t *cx_pkt;
  SvcInternal_t *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || codec_ctx == NULL || si == NULL) {
    return AOM_CODEC_INVALID_PARAM;
  }

  svc_log_reset(svc_ctx);

  res =
      aom_codec_encode(codec_ctx, rawimg, pts, (uint32_t)duration, 0, deadline);
  if (res != AOM_CODEC_OK) {
    return res;
  }
  // save compressed data
  iter = NULL;
  while ((cx_pkt = aom_codec_get_cx_data(codec_ctx, &iter))) {
    switch (cx_pkt->kind) {
#if AOM_ENCODER_ABI_VERSION > (5 + AOM_CODEC_ABI_VERSION)
#if CONFIG_SPATIAL_SVC
      case AOM_CODEC_SPATIAL_SVC_LAYER_PSNR: {
        int i;
        for (i = 0; i < svc_ctx->spatial_layers; ++i) {
          int j;
          svc_log(svc_ctx, SVC_LOG_DEBUG,
                  "SVC frame: %d, layer: %d, PSNR(Total/Y/U/V): "
                  "%2.3f  %2.3f  %2.3f  %2.3f \n",
                  si->psnr_pkt_received, i, cx_pkt->data.layer_psnr[i].psnr[0],
                  cx_pkt->data.layer_psnr[i].psnr[1],
                  cx_pkt->data.layer_psnr[i].psnr[2],
                  cx_pkt->data.layer_psnr[i].psnr[3]);
          svc_log(svc_ctx, SVC_LOG_DEBUG,
                  "SVC frame: %d, layer: %d, SSE(Total/Y/U/V): "
                  "%2.3f  %2.3f  %2.3f  %2.3f \n",
                  si->psnr_pkt_received, i, cx_pkt->data.layer_psnr[i].sse[0],
                  cx_pkt->data.layer_psnr[i].sse[1],
                  cx_pkt->data.layer_psnr[i].sse[2],
                  cx_pkt->data.layer_psnr[i].sse[3]);

          for (j = 0; j < COMPONENTS; ++j) {
            si->psnr_sum[i][j] += cx_pkt->data.layer_psnr[i].psnr[j];
            si->sse_sum[i][j] += cx_pkt->data.layer_psnr[i].sse[j];
          }
        }
        ++si->psnr_pkt_received;
        break;
      }
      case AOM_CODEC_SPATIAL_SVC_LAYER_SIZES: {
        int i;
        for (i = 0; i < svc_ctx->spatial_layers; ++i)
          si->bytes_sum[i] += cx_pkt->data.layer_sizes[i];
        break;
      }
#endif
#endif
      default: { break; }
    }
  }

  return AOM_CODEC_OK;
}

const char *aom_svc_get_message(const SvcContext *svc_ctx) {
  const SvcInternal_t *const si = get_const_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL) return NULL;
  return si->message_buffer;
}

static double calc_psnr(double d) {
  if (d == 0) return 100;
  return -10.0 * log(d) / log(10.0);
}

// dump accumulated statistics and reset accumulated values
const char *aom_svc_dump_statistics(SvcContext *svc_ctx) {
  int number_of_frames;
  int i, j;
  uint32_t bytes_total = 0;
  double scale[COMPONENTS];
  double psnr[COMPONENTS];
  double mse[COMPONENTS];
  double y_scale;

  SvcInternal_t *const si = get_svc_internal(svc_ctx);
  if (svc_ctx == NULL || si == NULL) return NULL;

  svc_log_reset(svc_ctx);

  number_of_frames = si->psnr_pkt_received;
  if (number_of_frames <= 0) return aom_svc_get_message(svc_ctx);

  svc_log(svc_ctx, SVC_LOG_INFO, "\n");
  for (i = 0; i < svc_ctx->spatial_layers; ++i) {
    svc_log(svc_ctx, SVC_LOG_INFO,
            "Layer %d Average PSNR=[%2.3f, %2.3f, %2.3f, %2.3f], Bytes=[%u]\n",
            i, (double)si->psnr_sum[i][0] / number_of_frames,
            (double)si->psnr_sum[i][1] / number_of_frames,
            (double)si->psnr_sum[i][2] / number_of_frames,
            (double)si->psnr_sum[i][3] / number_of_frames, si->bytes_sum[i]);
    // the following psnr calculation is deduced from ffmpeg.c#print_report
    y_scale = si->width * si->height * 255.0 * 255.0 * number_of_frames;
    scale[1] = y_scale;
    scale[2] = scale[3] = y_scale / 4;  // U or V
    scale[0] = y_scale * 1.5;           // total

    for (j = 0; j < COMPONENTS; j++) {
      psnr[j] = calc_psnr(si->sse_sum[i][j] / scale[j]);
      mse[j] = si->sse_sum[i][j] * 255.0 * 255.0 / scale[j];
    }
    svc_log(svc_ctx, SVC_LOG_INFO,
            "Layer %d Overall PSNR=[%2.3f, %2.3f, %2.3f, %2.3f]\n", i, psnr[0],
            psnr[1], psnr[2], psnr[3]);
    svc_log(svc_ctx, SVC_LOG_INFO,
            "Layer %d Overall MSE=[%2.3f, %2.3f, %2.3f, %2.3f]\n", i, mse[0],
            mse[1], mse[2], mse[3]);

    bytes_total += si->bytes_sum[i];
    // Clear sums for next time.
    si->bytes_sum[i] = 0;
    for (j = 0; j < COMPONENTS; ++j) {
      si->psnr_sum[i][j] = 0;
      si->sse_sum[i][j] = 0;
    }
  }

  // only display statistics once
  si->psnr_pkt_received = 0;

  svc_log(svc_ctx, SVC_LOG_INFO, "Total Bytes=[%u]\n", bytes_total);
  return aom_svc_get_message(svc_ctx);
}

void aom_svc_release(SvcContext *svc_ctx) {
  SvcInternal_t *si;
  if (svc_ctx == NULL) return;
  // do not use get_svc_internal as it will unnecessarily allocate an
  // SvcInternal_t if it was not already allocated
  si = (SvcInternal_t *)svc_ctx->internal;
  if (si != NULL) {
    free(si);
    svc_ctx->internal = NULL;
  }
}
