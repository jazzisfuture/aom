/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdint.h>

#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"

#include "aom_ports/system_state.h"

#if CONFIG_MISMATCH_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_MISMATCH_DEBUG

#include "av1/common/onyxc_int.h"
#include "av1/common/reconinter.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/encode_strategy.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/pass2_strategy.h"
#include "av1/encoder/temporal_filter.h"
#include "av1/encoder/tpl_model.h"

#define TEMPORAL_FILTER_KEY_FRAME (CONFIG_REALTIME_ONLY ? 0 : 1)

void av1_configure_buffer_updates(AV1_COMP *const cpi,
                                  EncodeFrameParams *const frame_params,
                                  const FRAME_UPDATE_TYPE type,
                                  int force_refresh_all) {
  // NOTE(weitinglin): Should we define another function to take care of
  // cpi->rc.is_$Source_Type to make this function as it is in the comment?

  cpi->rc.is_src_frame_alt_ref = 0;
  cpi->rc.is_src_frame_internal_arf = 0;

  switch (type) {
    case KF_UPDATE:
      frame_params->refresh_last_frame = 1;
      frame_params->refresh_golden_frame = 1;
      frame_params->refresh_bwd_ref_frame = 1;
      frame_params->refresh_alt2_ref_frame = 1;
      frame_params->refresh_alt_ref_frame = 1;
      break;

    case LF_UPDATE:
      frame_params->refresh_last_frame = 1;
      frame_params->refresh_golden_frame = 0;
      frame_params->refresh_bwd_ref_frame = 0;
      frame_params->refresh_alt2_ref_frame = 0;
      frame_params->refresh_alt_ref_frame = 0;
      break;

    case GF_UPDATE:
      // TODO(zoeliu): To further investigate whether 'refresh_last_frame' is
      //               needed.
      frame_params->refresh_last_frame = 1;
      frame_params->refresh_golden_frame = 1;
      frame_params->refresh_bwd_ref_frame = 0;
      frame_params->refresh_alt2_ref_frame = 0;
      frame_params->refresh_alt_ref_frame = 0;
      break;

    case OVERLAY_UPDATE:
      frame_params->refresh_last_frame = 0;
      frame_params->refresh_golden_frame = 1;
      frame_params->refresh_bwd_ref_frame = 0;
      frame_params->refresh_alt2_ref_frame = 0;
      frame_params->refresh_alt_ref_frame = 0;

      cpi->rc.is_src_frame_alt_ref = 1;
      break;

    case ARF_UPDATE:
      frame_params->refresh_last_frame = 0;
      frame_params->refresh_golden_frame = 0;
      // NOTE: BWDREF does not get updated along with ALTREF_FRAME.
      frame_params->refresh_bwd_ref_frame = 0;
      frame_params->refresh_alt2_ref_frame = 0;
      frame_params->refresh_alt_ref_frame = 1;
      break;

    case INTNL_OVERLAY_UPDATE:
      frame_params->refresh_last_frame = 1;
      frame_params->refresh_golden_frame = 0;
      frame_params->refresh_bwd_ref_frame = 0;
      frame_params->refresh_alt2_ref_frame = 0;
      frame_params->refresh_alt_ref_frame = 0;

      cpi->rc.is_src_frame_alt_ref = 1;
      cpi->rc.is_src_frame_internal_arf = 1;
      break;

    case INTNL_ARF_UPDATE:
      frame_params->refresh_last_frame = 0;
      frame_params->refresh_golden_frame = 0;
      if (!is_stat_generation_stage(cpi)) {
        frame_params->refresh_bwd_ref_frame = 1;
        frame_params->refresh_alt2_ref_frame = 0;
      } else {
        frame_params->refresh_bwd_ref_frame = 0;
        frame_params->refresh_alt2_ref_frame = 1;
      }
      frame_params->refresh_alt_ref_frame = 0;
      break;

    default: assert(0); break;
  }

  if (cpi->ext_refresh_frame_flags_pending &&
      (!is_stat_generation_stage(cpi))) {
    frame_params->refresh_last_frame = cpi->ext_refresh_last_frame;
    frame_params->refresh_golden_frame = cpi->ext_refresh_golden_frame;
    frame_params->refresh_alt_ref_frame = cpi->ext_refresh_alt_ref_frame;
    frame_params->refresh_bwd_ref_frame = cpi->ext_refresh_bwd_ref_frame;
    frame_params->refresh_alt2_ref_frame = cpi->ext_refresh_alt2_ref_frame;
  }

  if (force_refresh_all) {
    frame_params->refresh_last_frame = 1;
    frame_params->refresh_golden_frame = 1;
    frame_params->refresh_bwd_ref_frame = 1;
    frame_params->refresh_alt2_ref_frame = 1;
    frame_params->refresh_alt_ref_frame = 1;
  }
}

static void set_additional_frame_flags(const AV1_COMMON *const cm,
                                       unsigned int *const frame_flags) {
  if (frame_is_intra_only(cm)) *frame_flags |= FRAMEFLAGS_INTRAONLY;
  if (frame_is_sframe(cm)) *frame_flags |= FRAMEFLAGS_SWITCH;
  if (cm->error_resilient_mode) *frame_flags |= FRAMEFLAGS_ERROR_RESILIENT;
}

static INLINE void update_keyframe_counters(AV1_COMP *cpi) {
  if (cpi->common.show_frame) {
    if (!cpi->common.show_existing_frame || cpi->rc.is_src_frame_alt_ref ||
        cpi->common.current_frame.frame_type == KEY_FRAME) {
      // If this is a show_existing_frame with a source other than altref,
      // or if it is not a displayed forward keyframe, the keyframe update
      // counters were incremented when it was originally encoded.
      cpi->rc.frames_since_key++;
      cpi->rc.frames_to_key--;
    }
  }
}

static INLINE int is_frame_droppable(const AV1_COMP *const cpi) {
  return !(cpi->refresh_alt_ref_frame || cpi->refresh_alt2_ref_frame ||
           cpi->refresh_bwd_ref_frame || cpi->refresh_golden_frame ||
           cpi->refresh_last_frame);
}

static INLINE void update_frames_till_gf_update(AV1_COMP *cpi) {
  // TODO(weitinglin): Updating this counter for is_frame_droppable
  // is a work-around to handle the condition when a frame is drop.
  // We should fix the cpi->common.show_frame flag
  // instead of checking the other condition to update the counter properly.
  if (cpi->common.show_frame || is_frame_droppable(cpi)) {
    // Decrement count down till next gf
    if (cpi->rc.frames_till_gf_update_due > 0)
      cpi->rc.frames_till_gf_update_due--;
  }
}

static INLINE void update_gf_group_index(AV1_COMP *cpi) {
  // Increment the gf group index ready for the next frame. If this is
  // a show_existing_frame with a source other than altref, or if it is not
  // a displayed forward keyframe, the index was incremented when it was
  // originally encoded.
  if (!cpi->common.show_existing_frame || cpi->rc.is_src_frame_alt_ref ||
      cpi->common.current_frame.frame_type == KEY_FRAME) {
    ++cpi->gf_group.index;
  }
}

static void update_rc_counts(AV1_COMP *cpi) {
  update_keyframe_counters(cpi);
  update_frames_till_gf_update(cpi);
  update_gf_group_index(cpi);
}

// Get update type of the current frame.
static INLINE FRAME_UPDATE_TYPE get_frame_update_type(const AV1_COMP *cpi) {
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const int is_fwd_kf = is_show_existing_fwd_kf(cpi);
  if (gf_group->size == 0 || is_fwd_kf) {
    // Special case 1: happens at the first frame of a video.
    return KF_UPDATE;
  }
  if (gf_group->index == gf_group->size) {
    // Special case 2: happens at the start of next GF group, or at the end of
    // the key-frame group. So, not marked in gf_group->update_type array, but
    // can be inferred implicitly.
    return cpi->rc.source_alt_ref_active ? OVERLAY_UPDATE : GF_UPDATE;
  }
  // General case.
  return gf_group->update_type[gf_group->index];
}

// Get ARF update index for the current frame.
// Returns 1 for internal ARF, and 0 otherwise (including top-level ARF).
static INLINE int get_arf_update_idx(const GF_GROUP *const gf_group) {
  if (gf_group->index == gf_group->size) {
    // Special case: happens at the start of a video, or at the start of next GF
    // group, or at the end of the key-frame group. So, not marked in
    // gf_group->arf_update_idx array, but can implicitly inferred be a
    // top-level ARF.
    return 0;
  }
  // General case.
  return (gf_group->arf_update_idx[gf_group->index] > 0);
}

// Note: the parameters related to OVERLAY_UPDATE will be taken care of in
// av1_get_second_pass_params().
static void check_show_existing_frame(AV1_COMP *const cpi,
                                      EncodeFrameParams *const frame_params) {
  const GF_GROUP *const gf_group = &cpi->gf_group;
  assert(gf_group->index <= gf_group->size);
  AV1_COMMON *const cm = &cpi->common;
  if (cm->show_existing_frame == 1) {
    frame_params->show_existing_frame = 0;
  } else {
    const FRAME_UPDATE_TYPE frame_update_type = get_frame_update_type(cpi);
    const int which_arf = get_arf_update_idx(gf_group);
    const int is_fwd_kf = is_show_existing_fwd_kf(cpi);
    if (((frame_update_type == OVERLAY_UPDATE ||
          frame_update_type == INTNL_OVERLAY_UPDATE) &&
         cpi->is_arf_filter_off[which_arf]) ||
        is_fwd_kf) {
      frame_params->show_existing_frame = 1;
      frame_params->existing_fb_idx_to_show =
          (frame_update_type == OVERLAY_UPDATE ||
           frame_update_type == KEY_FRAME)
              ? get_ref_frame_map_idx(cm, ALTREF_FRAME)
              : get_ref_frame_map_idx(cm, BWDREF_FRAME);

    } else {
      frame_params->show_existing_frame = 0;
    }
  }
}

static void set_ext_overrides(AV1_COMP *const cpi,
                              EncodeFrameParams *const frame_params) {
  // Overrides the defaults with the externally supplied values with
  // av1_update_reference() and av1_update_entropy() calls
  // Note: The overrides are valid only for the next frame passed
  // to av1_encode_lowlevel()

  AV1_COMMON *const cm = &cpi->common;

  if (cpi->ext_use_s_frame) {
    frame_params->frame_type = S_FRAME;
  }

  if (cpi->ext_refresh_frame_context_pending) {
    cm->refresh_frame_context = cpi->ext_refresh_frame_context;
    cpi->ext_refresh_frame_context_pending = 0;
  }
  cm->allow_ref_frame_mvs = cpi->ext_use_ref_frame_mvs;

  frame_params->error_resilient_mode = cpi->ext_use_error_resilient;
  // A keyframe is already error resilient and keyframes with
  // error_resilient_mode interferes with the use of show_existing_frame
  // when forward reference keyframes are enabled.
  frame_params->error_resilient_mode &= frame_params->frame_type != KEY_FRAME;
  // For bitstream conformance, s-frames must be error-resilient
  frame_params->error_resilient_mode |= frame_params->frame_type == S_FRAME;
}

static int get_ref_frame_flags(const AV1_COMP *const cpi) {
  const AV1_COMMON *const cm = &cpi->common;

  const RefCntBuffer *last_buf = get_ref_frame_buf(cm, LAST_FRAME);
  const RefCntBuffer *last2_buf = get_ref_frame_buf(cm, LAST2_FRAME);
  const RefCntBuffer *last3_buf = get_ref_frame_buf(cm, LAST3_FRAME);
  const RefCntBuffer *golden_buf = get_ref_frame_buf(cm, GOLDEN_FRAME);
  const RefCntBuffer *bwd_buf = get_ref_frame_buf(cm, BWDREF_FRAME);
  const RefCntBuffer *alt2_buf = get_ref_frame_buf(cm, ALTREF2_FRAME);
  const RefCntBuffer *alt_buf = get_ref_frame_buf(cm, ALTREF_FRAME);

  // No.1 Priority: LAST_FRAME
  const int last2_is_last = (last2_buf == last_buf);
  const int last3_is_last = (last3_buf == last_buf);
  const int gld_is_last = (golden_buf == last_buf);
  const int bwd_is_last = (bwd_buf == last_buf);
  const int alt2_is_last = (alt2_buf == last_buf);
  const int alt_is_last = (alt_buf == last_buf);

  // No.2 Priority: ALTREF_FRAME
  const int last2_is_alt = (last2_buf == alt_buf);
  const int last3_is_alt = (last3_buf == alt_buf);
  const int gld_is_alt = (golden_buf == alt_buf);
  const int bwd_is_alt = (bwd_buf == alt_buf);
  const int alt2_is_alt = (alt2_buf == alt_buf);

  // No.3 Priority: LAST2_FRAME
  const int last3_is_last2 = (last3_buf == last2_buf);
  const int gld_is_last2 = (golden_buf == last2_buf);
  const int bwd_is_last2 = (bwd_buf == last2_buf);
  const int alt2_is_last2 = (alt2_buf == last2_buf);

  // No.4 Priority: LAST3_FRAME
  const int gld_is_last3 = (golden_buf == last3_buf);
  const int bwd_is_last3 = (bwd_buf == last3_buf);
  const int alt2_is_last3 = (alt2_buf == last3_buf);

  // No.5 Priority: GOLDEN_FRAME
  const int bwd_is_gld = (bwd_buf == golden_buf);
  const int alt2_is_gld = (alt2_buf == golden_buf);

  // No.6 Priority: BWDREF_FRAME
  const int alt2_is_bwd = (alt2_buf == bwd_buf);

  // No.7 Priority: ALTREF2_FRAME

  // cpi->ext_ref_frame_flags allows certain reference types to be disabled
  // by the external interface.  These are set by av1_apply_encoding_flags().
  // Start with what the external interface allows, then suppress any reference
  // types which we have found to be duplicates.

  int flags = cpi->ext_ref_frame_flags;

  if (cpi->rc.frames_till_gf_update_due == INT_MAX) flags &= ~AOM_GOLD_FLAG;

  if (alt_is_last) flags &= ~AOM_ALT_FLAG;

  if (last2_is_last || last2_is_alt) flags &= ~AOM_LAST2_FLAG;

  if (last3_is_last || last3_is_alt || last3_is_last2) flags &= ~AOM_LAST3_FLAG;

  if (gld_is_last || gld_is_last2 || gld_is_last3) {
    flags &= ~AOM_GOLD_FLAG;
  }

  if (!cpi->sf.use_fast_nonrd_pick_mode && gld_is_alt) {
    flags &= ~AOM_GOLD_FLAG;
  }

  if ((bwd_is_last || bwd_is_alt || bwd_is_last2 || bwd_is_last3 || bwd_is_gld))
    flags &= ~AOM_BWD_FLAG;

  if ((alt2_is_last || alt2_is_alt || alt2_is_last2 || alt2_is_last3 ||
       alt2_is_gld || alt2_is_bwd))
    flags &= ~AOM_ALT2_FLAG;

  return flags;
}

static int get_current_frame_ref_type(
    const AV1_COMP *const cpi, const EncodeFrameParams *const frame_params) {
  // We choose the reference "type" of this frame from the flags which indicate
  // which reference frames will be refreshed by it.  More than one  of these
  // flags may be set, so the order here implies an order of precedence. This is
  // just used to choose the primary_ref_frame (as the most recent reference
  // buffer of the same reference-type as the current frame)

  (void)frame_params;
  // TODO(jingning): This table should be a lot simpler with the new
  // ARF system in place. Keep frame_params for the time being as we are
  // still evaluating a few design options.
  switch (cpi->gf_group.layer_depth[cpi->gf_group.index]) {
    case 0: return 0;
    case 1: return 1;
    case MAX_ARF_LAYERS:
    case MAX_ARF_LAYERS + 1: return 4;
    default: return 7;
  }
}

static int choose_primary_ref_frame(
    const AV1_COMP *const cpi, const EncodeFrameParams *const frame_params) {
  const AV1_COMMON *const cm = &cpi->common;

  const int intra_only = frame_params->frame_type == KEY_FRAME ||
                         frame_params->frame_type == INTRA_ONLY_FRAME;
  if (intra_only || frame_params->error_resilient_mode ||
      cpi->ext_use_primary_ref_none) {
    return PRIMARY_REF_NONE;
  }

  // Find the most recent reference frame with the same reference type as the
  // current frame
  FRAME_CONTEXT_INDEX current_ref_type =
      get_current_frame_ref_type(cpi, frame_params);
  int wanted_fb = cpi->fb_of_context_type[current_ref_type];

  int primary_ref_frame = PRIMARY_REF_NONE;
  for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
    if (get_ref_frame_map_idx(cm, ref_frame) == wanted_fb) {
      primary_ref_frame = ref_frame - LAST_FRAME;
    }
  }

  return primary_ref_frame;
}

static void update_fb_of_context_type(
    const AV1_COMP *const cpi, const EncodeFrameParams *const frame_params,
    int *const fb_of_context_type) {
  const AV1_COMMON *const cm = &cpi->common;
  const int current_frame_ref_type =
      get_current_frame_ref_type(cpi, frame_params);

  if (frame_is_intra_only(cm) || cm->error_resilient_mode ||
      cpi->ext_use_primary_ref_none) {
    for (int i = 0; i < REF_FRAMES; i++) {
      fb_of_context_type[i] = -1;
    }
    fb_of_context_type[current_frame_ref_type] =
        cm->show_frame ? get_ref_frame_map_idx(cm, GOLDEN_FRAME)
                       : get_ref_frame_map_idx(cm, ALTREF_FRAME);
  }

  if (!encode_show_existing_frame(cm)) {
    // Refresh fb_of_context_type[]: see encoder.h for explanation
    if (cm->current_frame.frame_type == KEY_FRAME) {
      // All ref frames are refreshed, pick one that will live long enough
      fb_of_context_type[current_frame_ref_type] = 0;
    } else {
      // If more than one frame is refreshed, it doesn't matter which one we
      // pick so pick the first.  LST sometimes doesn't refresh any: this is ok

      for (int i = 0; i < REF_FRAMES; i++) {
        if (cm->current_frame.refresh_frame_flags & (1 << i)) {
          fb_of_context_type[current_frame_ref_type] = i;
          break;
        }
      }
    }
  }
}

static int get_order_offset(const GF_GROUP *const gf_group,
                            const EncodeFrameParams *const frame_params) {
  // shown frame by definition has order offset 0
  // show_existing_frame ignores order_offset and simply takes the order_hint
  // from the reference frame being shown.
  if (frame_params->show_frame || frame_params->show_existing_frame) return 0;

  const int arf_offset =
      AOMMIN((MAX_GF_INTERVAL - 1), gf_group->arf_src_offset[gf_group->index]);
  return AOMMIN((MAX_GF_INTERVAL - 1), arf_offset);
}

static void adjust_frame_rate(AV1_COMP *cpi,
                              const struct lookahead_entry *source) {
  int64_t this_duration;
  int step = 0;

  // Clear down mmx registers
  aom_clear_system_state();

  if (source->ts_start == cpi->first_time_stamp_ever) {
    this_duration = source->ts_end - source->ts_start;
    step = 1;
  } else {
    int64_t last_duration =
        cpi->last_end_time_stamp_seen - cpi->last_time_stamp_seen;

    this_duration = source->ts_end - cpi->last_end_time_stamp_seen;

    // do a step update if the duration changes by 10%
    if (last_duration)
      step = (int)((this_duration - last_duration) * 10 / last_duration);
  }

  if (this_duration) {
    if (step) {
      av1_new_framerate(cpi, 10000000.0 / this_duration);
    } else {
      // Average this frame's rate into the last second's average
      // frame rate. If we haven't seen 1 second yet, then average
      // over the whole interval seen.
      const double interval = AOMMIN(
          (double)(source->ts_end - cpi->first_time_stamp_ever), 10000000.0);
      double avg_duration = 10000000.0 / cpi->framerate;
      avg_duration *= (interval - avg_duration + this_duration);
      avg_duration /= interval;

      av1_new_framerate(cpi, 10000000.0 / avg_duration);
    }
  }
  cpi->last_time_stamp_seen = source->ts_start;
  cpi->last_end_time_stamp_seen = source->ts_end;
}

// If this is an alt-ref, returns the offset of the source frame used
// as the arf midpoint. Otherwise, returns 0.
static int get_arf_src_index(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  int arf_src_index = 0;
  if (!is_stat_generation_stage(cpi)) {
    const GF_GROUP *const gf_group = &cpi->gf_group;
    if (get_frame_update_type(cpi) == ARF_UPDATE) {
      assert(is_altref_enabled(cpi));
      arf_src_index = gf_group->arf_src_offset[gf_group->index];
    }
  } else if (rc->source_alt_ref_pending) {
    arf_src_index = rc->frames_till_gf_update_due;
  }
  return arf_src_index;
}

// If this is an internal alt-ref, returns the offset of the source frame used
// as the internal arf midpoint. Otherwise, returns 0.
static int get_internal_arf_src_index(AV1_COMP *cpi) {
  int internal_arf_src_index = 0;
  if (!is_stat_generation_stage(cpi)) {
    const GF_GROUP *const gf_group = &cpi->gf_group;
    if (gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE) {
      assert(is_altref_enabled(cpi) && cpi->internal_altref_allowed);
      internal_arf_src_index = gf_group->arf_src_offset[gf_group->index];
    }
  }
  return internal_arf_src_index;
}

// Called if this frame is an ARF or ARF2. Also handles forward-keyframes
// For an ARF set arf2=0, for ARF2 set arf2=1
// temporal_filtered is set to 1 if we temporally filter the ARF frame, so that
// the correct post-filter buffer can be used.
static struct lookahead_entry *setup_arf_or_arf2(
    AV1_COMP *const cpi, const int arf_src_index, const int arf2,
    int *temporal_filtered, EncodeFrameParams *const frame_params) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
#if !CONFIG_REALTIME_ONLY
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
#endif

  assert(arf_src_index <= rc->frames_to_key);
  *temporal_filtered = 0;

  struct lookahead_entry *source =
      av1_lookahead_peek(cpi->lookahead, arf_src_index, cpi->compressor_stage);

  if (source != NULL) {
    cm->showable_frame = 1;
    cpi->alt_ref_source = source;

    // When arf_src_index == rc->frames_to_key, it indicates a fwd_kf
    if (!arf2 && arf_src_index == rc->frames_to_key) {
      // Skip temporal filtering and mark as intra_only if we have a fwd_kf
      const int which_arf = get_arf_update_idx(&cpi->gf_group);
      cpi->is_arf_filter_off[which_arf] = 1;
      cpi->no_show_kf = 1;
    } else {
#if !CONFIG_REALTIME_ONLY
      if (oxcf->arnr_max_frames > 0) {
        // Produce the filtered ARF frame.
        av1_temporal_filter(cpi, arf_src_index);
        aom_extend_frame_borders(&cpi->alt_ref_buffer, av1_num_planes(cm));
        *temporal_filtered = 1;
      }
#endif
    }
    frame_params->show_frame = 0;
  }
  rc->source_alt_ref_pending = 0;
  return source;
}

// Determine whether there is a forced keyframe pending in the lookahead buffer
static int is_forced_keyframe_pending(struct lookahead_ctx *lookahead,
                                      const int up_to_index,
                                      const COMPRESSOR_STAGE compressor_stage) {
  for (int i = 0; i <= up_to_index; i++) {
    const struct lookahead_entry *e =
        av1_lookahead_peek(lookahead, i, compressor_stage);
    if (e == NULL) {
      // We have reached the end of the lookahead buffer and not early-returned
      // so there isn't a forced key-frame pending.
      return 0;
    } else if (e->flags == AOM_EFLAG_FORCE_KF) {
      return 1;
    } else {
      continue;
    }
  }
  return 0;  // Never reached
}

// Check if we should encode an ARF or internal ARF.  If not, try a LAST
// Do some setup associated with the chosen source
// temporal_filtered, flush, and frame_update_type are outputs.
// Return the frame source, or NULL if we couldn't find one
static struct lookahead_entry *choose_frame_source(
    AV1_COMP *const cpi, int *const temporal_filtered, int *const flush,
    struct lookahead_entry **last_source, FRAME_UPDATE_TYPE *frame_update_type,
    EncodeFrameParams *const frame_params) {
  AV1_COMMON *const cm = &cpi->common;
  struct lookahead_entry *source = NULL;
  *temporal_filtered = 0;

  // Should we encode an alt-ref frame.
  int arf_src_index = get_arf_src_index(cpi);
  if (arf_src_index && is_forced_keyframe_pending(cpi->lookahead, arf_src_index,
                                                  cpi->compressor_stage)) {
    arf_src_index = 0;
    *flush = 1;
  }

  if (arf_src_index) {
    source = setup_arf_or_arf2(cpi, arf_src_index, 0, temporal_filtered,
                               frame_params);
    *frame_update_type = ARF_UPDATE;
  }

  // Should we encode an internal Alt-ref frame (mutually exclusive to ARF)
  arf_src_index = get_internal_arf_src_index(cpi);
  if (arf_src_index && is_forced_keyframe_pending(cpi->lookahead, arf_src_index,
                                                  cpi->compressor_stage)) {
    arf_src_index = 0;
    *flush = 1;
  }

  if (arf_src_index) {
    source = setup_arf_or_arf2(cpi, arf_src_index, 1, temporal_filtered,
                               frame_params);
    *frame_update_type = INTNL_ARF_UPDATE;
  }

  if (!source) {
    // Get last frame source.
    if (cm->current_frame.frame_number > 0) {
      *last_source =
          av1_lookahead_peek(cpi->lookahead, -1, cpi->compressor_stage);
    }
    // Read in the source frame.
    source = av1_lookahead_pop(cpi->lookahead, *flush, cpi->compressor_stage);
    if (source == NULL) return NULL;
    *frame_update_type = LF_UPDATE;  // Default update type
    frame_params->show_frame = 1;

    // Check to see if the frame should be encoded as an arf overlay.
    if (cpi->alt_ref_source == source) {
      *frame_update_type = OVERLAY_UPDATE;
      cpi->alt_ref_source = NULL;
    }
  }
  return source;
}

// Don't allow a show_existing_frame to coincide with an error resilient or
// S-Frame. An exception can be made in the case of a keyframe, since it does
// not depend on any previous frames.
static int allow_show_existing(const AV1_COMP *const cpi,
                               unsigned int frame_flags) {
  if (cpi->common.current_frame.frame_number == 0) return 0;

  const struct lookahead_entry *lookahead_src =
      av1_lookahead_peek(cpi->lookahead, 0, cpi->compressor_stage);
  if (lookahead_src == NULL) return 1;

  const int is_error_resilient =
      cpi->oxcf.error_resilient_mode ||
      (lookahead_src->flags & AOM_EFLAG_ERROR_RESILIENT);
  const int is_s_frame =
      cpi->oxcf.s_frame_mode || (lookahead_src->flags & AOM_EFLAG_SET_S_FRAME);
  const int is_key_frame =
      (cpi->rc.frames_to_key == 0) || (frame_flags & FRAMEFLAGS_KEY);
  return !(is_error_resilient || is_s_frame) || is_key_frame;
}

// Update frame_flags to tell the encoder's caller what sort of frame was
// encoded.
static void update_frame_flags(AV1_COMP *cpi, unsigned int *frame_flags) {
  if (encode_show_existing_frame(&cpi->common)) {
    *frame_flags &= ~FRAMEFLAGS_GOLDEN;
    *frame_flags &= ~FRAMEFLAGS_BWDREF;
    *frame_flags &= ~FRAMEFLAGS_ALTREF;
    *frame_flags &= ~FRAMEFLAGS_KEY;
    return;
  }

  if (cpi->refresh_golden_frame == 1) {
    *frame_flags |= FRAMEFLAGS_GOLDEN;
  } else {
    *frame_flags &= ~FRAMEFLAGS_GOLDEN;
  }

  if (cpi->refresh_alt_ref_frame == 1) {
    *frame_flags |= FRAMEFLAGS_ALTREF;
  } else {
    *frame_flags &= ~FRAMEFLAGS_ALTREF;
  }

  if (cpi->refresh_bwd_ref_frame == 1) {
    *frame_flags |= FRAMEFLAGS_BWDREF;
  } else {
    *frame_flags &= ~FRAMEFLAGS_BWDREF;
  }

  if (cpi->common.current_frame.frame_type == KEY_FRAME) {
    *frame_flags |= FRAMEFLAGS_KEY;
  } else {
    *frame_flags &= ~FRAMEFLAGS_KEY;
  }
}

#define DUMP_REF_FRAME_IMAGES 0

#if DUMP_REF_FRAME_IMAGES == 1
static int dump_one_image(AV1_COMMON *cm,
                          const YV12_BUFFER_CONFIG *const ref_buf,
                          char *file_name) {
  int h;
  FILE *f_ref = NULL;

  if (ref_buf == NULL) {
    printf("Frame data buffer is NULL.\n");
    return AOM_CODEC_MEM_ERROR;
  }

  if ((f_ref = fopen(file_name, "wb")) == NULL) {
    printf("Unable to open file %s to write.\n", file_name);
    return AOM_CODEC_MEM_ERROR;
  }

  // --- Y ---
  for (h = 0; h < cm->height; ++h) {
    fwrite(&ref_buf->y_buffer[h * ref_buf->y_stride], 1, cm->width, f_ref);
  }
  // --- U ---
  for (h = 0; h < (cm->height >> 1); ++h) {
    fwrite(&ref_buf->u_buffer[h * ref_buf->uv_stride], 1, (cm->width >> 1),
           f_ref);
  }
  // --- V ---
  for (h = 0; h < (cm->height >> 1); ++h) {
    fwrite(&ref_buf->v_buffer[h * ref_buf->uv_stride], 1, (cm->width >> 1),
           f_ref);
  }

  fclose(f_ref);

  return AOM_CODEC_OK;
}

static void dump_ref_frame_images(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  MV_REFERENCE_FRAME ref_frame;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    char file_name[256] = "";
    snprintf(file_name, sizeof(file_name), "/tmp/enc_F%d_ref_%d.yuv",
             cm->current_frame.frame_number, ref_frame);
    dump_one_image(cm, get_ref_frame_yv12_buf(cpi, ref_frame), file_name);
  }
}
#endif  // DUMP_REF_FRAME_IMAGES == 1

int av1_get_refresh_ref_frame_map(int refresh_frame_flags) {
  int ref_map_index = INVALID_IDX;

  for (ref_map_index = 0; ref_map_index < REF_FRAMES; ++ref_map_index)
    if ((refresh_frame_flags >> ref_map_index) & 1) break;

  return ref_map_index;
}

static void update_arf_stack(AV1_COMP *cpi, int ref_map_index,
                             RefBufferStack *ref_buffer_stack) {
  (void)cpi;
  if (ref_buffer_stack->arf_stack_size >= 0) {
    if (ref_buffer_stack->arf_stack[0] == ref_map_index)
      stack_pop(ref_buffer_stack->arf_stack, &ref_buffer_stack->arf_stack_size);
  }

  if (ref_buffer_stack->lst_stack_size) {
    for (int i = ref_buffer_stack->lst_stack_size - 1; i >= 0; --i) {
      if (ref_buffer_stack->lst_stack[i] == ref_map_index) {
        for (int idx = i; idx < ref_buffer_stack->lst_stack_size - 1; ++idx)
          ref_buffer_stack->lst_stack[idx] =
              ref_buffer_stack->lst_stack[idx + 1];
        ref_buffer_stack->lst_stack[i] = INVALID_IDX;
        --ref_buffer_stack->lst_stack_size;
      }
    }
  }

  if (ref_buffer_stack->gld_stack_size) {
    for (int i = ref_buffer_stack->gld_stack_size - 1; i >= 0; --i) {
      if (ref_buffer_stack->gld_stack[i] == ref_map_index) {
        for (int idx = i; idx < ref_buffer_stack->gld_stack_size - 1; ++idx)
          ref_buffer_stack->gld_stack[idx] =
              ref_buffer_stack->gld_stack[idx + 1];
        ref_buffer_stack->gld_stack[i] = INVALID_IDX;
        --ref_buffer_stack->gld_stack_size;
      }
    }
  }
}

// Update reference frame stack info.
void av1_update_ref_frame_map(AV1_COMP *cpi,
                              FRAME_UPDATE_TYPE frame_update_type,
                              int show_existing, int ref_map_index,
                              RefBufferStack *ref_buffer_stack) {
  AV1_COMMON *const cm = &cpi->common;
  // TODO(jingning): Consider the S-frame same as key frame for the
  // reference frame tracking purpose. The logic might be better
  // expressed than converting the frame update type.
  if (frame_is_sframe(cm)) frame_update_type = KEY_FRAME;

  if (is_frame_droppable(cpi)) return;

  switch (frame_update_type) {
    case KEY_FRAME:
      if (show_existing)
        ref_map_index = stack_pop(ref_buffer_stack->arf_stack,
                                  &ref_buffer_stack->arf_stack_size);
      stack_reset(ref_buffer_stack->lst_stack,
                  &ref_buffer_stack->lst_stack_size);
      stack_reset(ref_buffer_stack->gld_stack,
                  &ref_buffer_stack->gld_stack_size);
      stack_reset(ref_buffer_stack->arf_stack,
                  &ref_buffer_stack->arf_stack_size);
      stack_push(ref_buffer_stack->gld_stack, &ref_buffer_stack->gld_stack_size,
                 ref_map_index);
      break;
    case GF_UPDATE:
      update_arf_stack(cpi, ref_map_index, ref_buffer_stack);
      stack_push(ref_buffer_stack->gld_stack, &ref_buffer_stack->gld_stack_size,
                 ref_map_index);
      break;
    case LF_UPDATE:
      update_arf_stack(cpi, ref_map_index, ref_buffer_stack);
      stack_push(ref_buffer_stack->lst_stack, &ref_buffer_stack->lst_stack_size,
                 ref_map_index);
      break;
    case ARF_UPDATE:
    case INTNL_ARF_UPDATE:
      update_arf_stack(cpi, ref_map_index, ref_buffer_stack);
      stack_push(ref_buffer_stack->arf_stack, &ref_buffer_stack->arf_stack_size,
                 ref_map_index);
      break;
    case OVERLAY_UPDATE:
      ref_map_index = stack_pop(ref_buffer_stack->arf_stack,
                                &ref_buffer_stack->arf_stack_size);
      stack_push(ref_buffer_stack->gld_stack, &ref_buffer_stack->gld_stack_size,
                 ref_map_index);
      break;
    case INTNL_OVERLAY_UPDATE:
      ref_map_index = stack_pop(ref_buffer_stack->arf_stack,
                                &ref_buffer_stack->arf_stack_size);
      stack_push(ref_buffer_stack->lst_stack, &ref_buffer_stack->lst_stack_size,
                 ref_map_index);
      break;
    default: assert(0 && "unknown type");
  }

  return;
}

static int get_free_ref_map_index(const RefBufferStack *ref_buffer_stack) {
  for (int idx = 0; idx < REF_FRAMES; ++idx) {
    int is_free = 1;
    for (int i = 0; i < ref_buffer_stack->arf_stack_size; ++i) {
      if (ref_buffer_stack->arf_stack[i] == idx) {
        is_free = 0;
        break;
      }
    }

    for (int i = 0; i < ref_buffer_stack->lst_stack_size; ++i) {
      if (ref_buffer_stack->lst_stack[i] == idx) {
        is_free = 0;
        break;
      }
    }

    for (int i = 0; i < ref_buffer_stack->gld_stack_size; ++i) {
      if (ref_buffer_stack->gld_stack[i] == idx) {
        is_free = 0;
        break;
      }
    }

    if (is_free) return idx;
  }
  return INVALID_IDX;
}

int av1_get_refresh_frame_flags(const AV1_COMP *const cpi,
                                const EncodeFrameParams *const frame_params,
                                FRAME_UPDATE_TYPE frame_update_type,
                                const RefBufferStack *const ref_buffer_stack) {
  const AV1_COMMON *const cm = &cpi->common;

  // Switch frames and shown key-frames overwrite all reference slots
  if ((frame_params->frame_type == KEY_FRAME && frame_params->show_frame) ||
      frame_params->frame_type == S_FRAME)
    return 0xFF;

  // show_existing_frames don't actually send refresh_frame_flags so set the
  // flags to 0 to keep things consistent.
  if (frame_params->show_existing_frame &&
      (!frame_params->error_resilient_mode ||
       frame_params->frame_type == KEY_FRAME)) {
    return 0;
  }

  if (is_frame_droppable(cpi)) return 0;

  int refresh_mask = 0;

  if (cpi->ext_refresh_frame_flags_pending) {
    // Unfortunately the encoder interface reflects the old refresh_*_frame
    // flags so we have to replicate the old refresh_frame_flags logic here in
    // order to preserve the behaviour of the flag overrides.
    int ref_frame_map_idx = get_ref_frame_map_idx(cm, LAST3_FRAME);
    if (ref_frame_map_idx != INVALID_IDX)
      refresh_mask |= cpi->ext_refresh_last_frame << ref_frame_map_idx;

    ref_frame_map_idx = get_ref_frame_map_idx(cm, EXTREF_FRAME);
    if (ref_frame_map_idx != INVALID_IDX)
      refresh_mask |= cpi->ext_refresh_bwd_ref_frame << ref_frame_map_idx;

    ref_frame_map_idx = get_ref_frame_map_idx(cm, ALTREF2_FRAME);
    if (ref_frame_map_idx != INVALID_IDX)
      refresh_mask |= cpi->ext_refresh_alt2_ref_frame << ref_frame_map_idx;

    if (frame_update_type == OVERLAY_UPDATE) {
      ref_frame_map_idx = get_ref_frame_map_idx(cm, ALTREF_FRAME);
      if (ref_frame_map_idx != INVALID_IDX)
        refresh_mask |= cpi->ext_refresh_golden_frame << ref_frame_map_idx;
    } else {
      ref_frame_map_idx = get_ref_frame_map_idx(cm, GOLDEN_FRAME);
      if (ref_frame_map_idx != INVALID_IDX)
        refresh_mask |= cpi->ext_refresh_golden_frame << ref_frame_map_idx;

      ref_frame_map_idx = get_ref_frame_map_idx(cm, ALTREF_FRAME);
      if (ref_frame_map_idx != INVALID_IDX)
        refresh_mask |= cpi->ext_refresh_alt_ref_frame << ref_frame_map_idx;
    }
    return refresh_mask;
  }

  // Search for the open slot to store the current frame.
  int free_fb_index = get_free_ref_map_index(ref_buffer_stack);
  switch (frame_update_type) {
    case KF_UPDATE:
    case GF_UPDATE:
      if (free_fb_index != INVALID_IDX) {
        refresh_mask = 1 << free_fb_index;
      } else {
        if (ref_buffer_stack->gld_stack_size)
          refresh_mask =
              1 << ref_buffer_stack
                       ->gld_stack[ref_buffer_stack->gld_stack_size - 1];
        else
          refresh_mask =
              1 << ref_buffer_stack
                       ->lst_stack[ref_buffer_stack->lst_stack_size - 1];
      }
      break;
    case LF_UPDATE:
      if (free_fb_index != INVALID_IDX) {
        refresh_mask = 1 << free_fb_index;
      } else {
        if (ref_buffer_stack->lst_stack_size >= 2)
          refresh_mask =
              1 << ref_buffer_stack
                       ->lst_stack[ref_buffer_stack->lst_stack_size - 1];
        else
          assert(0 && "No ref map index found");
      }
      break;
    case ARF_UPDATE:
      if (free_fb_index != INVALID_IDX) {
        refresh_mask = 1 << free_fb_index;
      } else {
        if (ref_buffer_stack->gld_stack_size >= 3)
          refresh_mask =
              1 << ref_buffer_stack
                       ->gld_stack[ref_buffer_stack->gld_stack_size - 1];
        else if (ref_buffer_stack->lst_stack_size >= 2)
          refresh_mask =
              1 << ref_buffer_stack
                       ->lst_stack[ref_buffer_stack->lst_stack_size - 1];
        else
          assert(0 && "No ref map index found");
      }
      break;
    case INTNL_ARF_UPDATE:
      if (free_fb_index != INVALID_IDX) {
        refresh_mask = 1 << free_fb_index;
      } else {
        refresh_mask =
            1 << ref_buffer_stack
                     ->lst_stack[ref_buffer_stack->lst_stack_size - 1];
      }
      break;
    case OVERLAY_UPDATE: break;
    case INTNL_OVERLAY_UPDATE: break;
    default: assert(0); break;
  }

  return refresh_mask;
}

#if !CONFIG_REALTIME_ONLY
void setup_mi(AV1_COMP *const cpi, YV12_BUFFER_CONFIG *src) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &cpi->td.mb;
  MACROBLOCKD *const xd = &x->e_mbd;

  CHROMA_REF_INFO sb_chr_ref_info = {
    1, 0, 0, 0, cm->seq_params.sb_size, cm->seq_params.sb_size
  };
  av1_setup_src_planes(x, src, 0, 0, num_planes, &sb_chr_ref_info);

  av1_setup_block_planes(xd, cm->seq_params.subsampling_x,
                         cm->seq_params.subsampling_y, num_planes);

  xd->mi = cm->mi_grid_base;
  xd->mi[0] = cm->mi;
  x->mbmi_ext = cpi->mbmi_ext_base;
}

// Apply temporal filtering to key frames and encode the filtered frame.
// If the current frame is not key frame, this function is identical to
// av1_encode().
static int denoise_and_encode(AV1_COMP *const cpi, uint8_t *const dest,
                              EncodeFrameInput *const frame_input,
                              EncodeFrameParams *const frame_params,
                              EncodeFrameResults *const frame_results,
                              int *temporal_filtered) {
  if (frame_params->frame_type != KEY_FRAME) {
    cpi->pack_bitstream = 1;
    if (av1_encode(cpi, dest, frame_input, frame_params, frame_results) !=
        AOM_CODEC_OK) {
      return AOM_CODEC_ERROR;
    }
    return AOM_CODEC_OK;
  }

  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  AV1_COMMON *const cm = &cpi->common;
  double noise_level;
  const int use_hbd = frame_input->source->flags & YV12_FLAG_HIGHBITDEPTH;
  if (use_hbd) {
    noise_level = highbd_estimate_noise(
        frame_input->source->y_buffer, frame_input->source->y_crop_width,
        frame_input->source->y_crop_height, frame_input->source->y_stride,
        cm->seq_params.bit_depth, EDGE_THRESHOLD);
  } else {
    noise_level = estimate_noise(frame_input->source->y_buffer,
                                 frame_input->source->y_crop_width,
                                 frame_input->source->y_crop_height,
                                 frame_input->source->y_stride, EDGE_THRESHOLD);
  }
  const int apply_filtering =
      !is_stat_generation_stage(cpi) && frame_params->frame_type == KEY_FRAME &&
      frame_params->show_frame &&
      cpi->rc.frames_to_key > NUM_KEY_FRAME_DENOISING && noise_level > 0 &&
      !is_lossless_requested(oxcf) && oxcf->arnr_max_frames > 0;

  // Apply filtering to key frame and encode.
  if (apply_filtering) {
    // Initialization for frame motion estimation.
    MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
    av1_init_context_buffers(cm);
    setup_mi(cpi, frame_input->source);
    av1_init_macroblockd(cm, xd, NULL);
    memset(cpi->mbmi_ext_base, 0,
           cm->mi_rows * cm->mi_cols * sizeof(*cpi->mbmi_ext_base));

    av1_set_speed_features_framesize_independent(cpi, oxcf->speed);
    av1_set_speed_features_framesize_dependent(cpi, oxcf->speed);
    av1_set_rd_speed_thresholds(cpi);
    av1_setup_frame_buf_refs(cm);
    av1_setup_frame_sign_bias(cm);
    av1_frame_init_quantizer(cpi);
    av1_setup_past_independence(cm);

    // Keep a copy of the source image.
    const int num_planes = av1_num_planes(cm);
    aom_yv12_copy_frame(frame_input->source, &cpi->source_kf_buffer,
                        num_planes);
    av1_temporal_filter(cpi, -1);
    aom_extend_frame_borders(&cpi->alt_ref_buffer, num_planes);
    // Use the filtered frame for encoding.
    frame_input->source = &cpi->alt_ref_buffer;
    *temporal_filtered = 1;
    cpi->pack_bitstream = 1;
    if (av1_encode(cpi, dest, frame_input, frame_params, frame_results) !=
        AOM_CODEC_OK) {
      return AOM_CODEC_ERROR;
    }
    // Set frame_input source to true source for psnr calculation.
    if (oxcf->arnr_max_frames > 0 && *temporal_filtered) {
      aom_yv12_copy_frame(&cpi->source_kf_buffer, cpi->source, num_planes);
      aom_yv12_copy_frame(&cpi->source_kf_buffer, cpi->unscaled_source,
                          num_planes);
    }
  } else {
    // Encode other frames.
    cpi->pack_bitstream = 1;
    if (av1_encode(cpi, dest, frame_input, frame_params, frame_results) !=
        AOM_CODEC_OK) {
      return AOM_CODEC_ERROR;
    }
  }
  return AOM_CODEC_OK;
}
#endif  // !CONFIG_REALTIME_ONLY

void av1_get_ref_frames(AV1_COMP *const cpi,
                        FRAME_UPDATE_TYPE frame_update_type,
                        RefBufferStack *ref_buffer_stack) {
  AV1_COMMON *cm = &cpi->common;
  (void)frame_update_type;

  const int arf_stack_size = ref_buffer_stack->arf_stack_size;
  const int lst_stack_size = ref_buffer_stack->lst_stack_size;
  const int gld_stack_size = ref_buffer_stack->gld_stack_size;

  // Initialization
  for (int i = 0; i < REF_FRAMES; ++i) cm->remapped_ref_idx[i] = INVALID_IDX;

  if (arf_stack_size) {
    cm->remapped_ref_idx[ALTREF_FRAME - LAST_FRAME] =
        ref_buffer_stack->arf_stack[arf_stack_size - 1];

    if (arf_stack_size > 1)
      cm->remapped_ref_idx[BWDREF_FRAME - LAST_FRAME] =
          ref_buffer_stack->arf_stack[0];

    if (arf_stack_size > 2)
      cm->remapped_ref_idx[ALTREF2_FRAME - LAST_FRAME] =
          ref_buffer_stack->arf_stack[1];
  }

  if (lst_stack_size) {
    cm->remapped_ref_idx[LAST_FRAME - LAST_FRAME] =
        ref_buffer_stack->lst_stack[0];

    if (lst_stack_size > 1)
      cm->remapped_ref_idx[LAST2_FRAME - LAST_FRAME] =
          ref_buffer_stack->lst_stack[1];
  }

  if (gld_stack_size) {
    cm->remapped_ref_idx[GOLDEN_FRAME - LAST_FRAME] =
        ref_buffer_stack->gld_stack[0];

    if (gld_stack_size > 1) {
      if (arf_stack_size <= 1)
        cm->remapped_ref_idx[BWDREF_FRAME - LAST_FRAME] =
            ref_buffer_stack->gld_stack[1];
      else
        cm->remapped_ref_idx[LAST3_FRAME - LAST_FRAME] =
            ref_buffer_stack->gld_stack[1];
    }
  }

  for (int idx = ALTREF_FRAME - LAST_FRAME; idx >= 0; --idx) {
    int ref_map_index = cm->remapped_ref_idx[idx];

    if (ref_map_index != INVALID_IDX) continue;

    for (int i = 0;
         i < ref_buffer_stack->arf_stack_size && ref_map_index == INVALID_IDX;
         ++i) {
      int ref_idx = 0;
      for (ref_idx = 0; ref_idx <= ALTREF_FRAME - LAST_FRAME; ++ref_idx)
        if (ref_buffer_stack->arf_stack[i] == cm->remapped_ref_idx[ref_idx])
          break;

      // not in use
      if (ref_idx > ALTREF_FRAME - LAST_FRAME) {
        ref_map_index = ref_buffer_stack->arf_stack[i];
        break;
      }
    }

    for (int i = 0;
         i < ref_buffer_stack->gld_stack_size && ref_map_index == INVALID_IDX;
         ++i) {
      int ref_idx = 0;
      for (ref_idx = 0; ref_idx <= ALTREF_FRAME - LAST_FRAME; ++ref_idx)
        if (ref_buffer_stack->gld_stack[i] == cm->remapped_ref_idx[ref_idx])
          break;

      // not in use
      if (ref_idx > ALTREF_FRAME - LAST_FRAME) {
        ref_map_index = ref_buffer_stack->gld_stack[i];
        break;
      }
    }

    for (int i = 0;
         i < ref_buffer_stack->lst_stack_size && ref_map_index == INVALID_IDX;
         ++i) {
      int ref_idx = 0;
      for (ref_idx = 0; ref_idx <= ALTREF_FRAME - LAST_FRAME; ++ref_idx)
        if (ref_buffer_stack->lst_stack[i] == cm->remapped_ref_idx[ref_idx])
          break;

      // not in use
      if (ref_idx > ALTREF_FRAME - LAST_FRAME) {
        ref_map_index = ref_buffer_stack->lst_stack[i];
        break;
      }
    }

    if (ref_map_index != INVALID_IDX)
      cm->remapped_ref_idx[idx] = ref_map_index;
    else
      cm->remapped_ref_idx[idx] = ref_buffer_stack->gld_stack[0];
  }

  return;
}

int av1_encode_strategy(AV1_COMP *const cpi, size_t *const size,
                        uint8_t *const dest, unsigned int *frame_flags,
                        int64_t *const time_stamp, int64_t *const time_end,
                        const aom_rational64_t *const timestamp_ratio,
                        int flush) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  AV1_COMMON *const cm = &cpi->common;

  EncodeFrameInput frame_input;
  EncodeFrameParams frame_params;
  EncodeFrameResults frame_results;
  memset(&frame_input, 0, sizeof(frame_input));
  memset(&frame_params, 0, sizeof(frame_params));
  memset(&frame_results, 0, sizeof(frame_results));

  // TODO(sarahparker) finish bit allocation for one pass pyramid
  if (has_no_stats_stage(cpi) && oxcf->rc_mode != AOM_Q) {
    cpi->oxcf.gf_max_pyr_height =
        AOMMIN(cpi->oxcf.gf_max_pyr_height, USE_ALTREF_FOR_ONE_PASS);
    cpi->oxcf.gf_min_pyr_height =
        AOMMIN(cpi->oxcf.gf_min_pyr_height, cpi->oxcf.gf_max_pyr_height);
  }

  if (!is_stat_generation_stage(cpi)) {
    check_show_existing_frame(cpi, &frame_params);
    frame_params.show_existing_frame &= allow_show_existing(cpi, *frame_flags);
  } else {
    frame_params.show_existing_frame = 0;
  }

  int temporal_filtered = 0;
  struct lookahead_entry *source = NULL;
  struct lookahead_entry *last_source = NULL;
  FRAME_UPDATE_TYPE frame_update_type;
  if (frame_params.show_existing_frame) {
    source = av1_lookahead_pop(cpi->lookahead, flush, cpi->compressor_stage);
    frame_update_type = LF_UPDATE;
  } else {
    source = choose_frame_source(cpi, &temporal_filtered, &flush, &last_source,
                                 &frame_update_type, &frame_params);
  }

  // In pass 0 and 2, we get the frame_update_type from gf_group
  if (!is_stat_generation_stage(cpi)) {
    frame_update_type = get_frame_update_type(cpi);
  }

  if (source == NULL) {  // If no source was found, we can't encode a frame.
#if !CONFIG_REALTIME_ONLY
    if (flush && oxcf->pass == 1 && !cpi->twopass.first_pass_done) {
      av1_end_first_pass(cpi); /* get last stats packet */
      cpi->twopass.first_pass_done = 1;
    }
#endif
    return -1;
  }

  frame_input.source = temporal_filtered ? &cpi->alt_ref_buffer : &source->img;
  frame_input.last_source = last_source != NULL ? &last_source->img : NULL;
  frame_input.ts_duration = source->ts_end - source->ts_start;

  *time_stamp = source->ts_start;
  *time_end = source->ts_end;
  if (source->ts_start < cpi->first_time_stamp_ever) {
    cpi->first_time_stamp_ever = source->ts_start;
    cpi->last_end_time_stamp_seen = source->ts_start;
  }

  av1_apply_encoding_flags(cpi, source->flags);
  if (!frame_params.show_existing_frame)
    *frame_flags = (source->flags & AOM_EFLAG_FORCE_KF) ? FRAMEFLAGS_KEY : 0;

  const int is_overlay = frame_params.show_existing_frame &&
                         (frame_update_type == OVERLAY_UPDATE ||
                          frame_update_type == INTNL_OVERLAY_UPDATE);
  if (frame_params.show_frame || is_overlay) {
    // Shown frames and arf-overlay frames need frame-rate considering
    adjust_frame_rate(cpi, source);
  }

  if (frame_params.show_existing_frame) {
    // show_existing_frame implies this frame is shown!
    frame_params.show_frame = 1;
  } else {
    if (cpi->film_grain_table) {
      cm->cur_frame->film_grain_params_present = aom_film_grain_table_lookup(
          cpi->film_grain_table, *time_stamp, *time_end, 0 /* =erase */,
          &cm->film_grain_params);
    } else {
      cm->cur_frame->film_grain_params_present =
          cm->seq_params.film_grain_params_present;
    }
    // only one operating point supported now
    const int64_t pts64 = ticks_to_timebase_units(timestamp_ratio, *time_stamp);
    if (pts64 < 0 || pts64 > UINT32_MAX) return AOM_CODEC_ERROR;
    cpi->common.frame_presentation_time = (uint32_t)pts64;
  }

#if CONFIG_REALTIME_ONLY
  av1_get_one_pass_rt_params(cpi, &frame_params, *frame_flags);
  frame_update_type = get_frame_update_type(cpi);
#else
  if (has_no_stats_stage(cpi) && oxcf->mode == REALTIME &&
      oxcf->lag_in_frames == 0) {
    av1_get_one_pass_rt_params(cpi, &frame_params, *frame_flags);
    frame_update_type = get_frame_update_type(cpi);
  } else if (!is_stat_generation_stage(cpi) &&
             (!frame_params.show_existing_frame || is_overlay ||
              (frame_params.show_existing_frame &&
               frame_params.frame_type == KEY_FRAME))) {
    // GF_GROUP needs updating for arf overlays as well as non-show-existing
    av1_get_second_pass_params(cpi, &frame_params, *frame_flags);
    frame_update_type = get_frame_update_type(cpi);
  }
#endif

  if (frame_params.show_existing_frame &&
      frame_params.frame_type != KEY_FRAME) {
    // Force show-existing frames to be INTER, except forward keyframes
    frame_params.frame_type = INTER_FRAME;
  }

  // TODO(david.turner@argondesign.com): Move all the encode strategy
  // (largely near av1_get_compressed_data) in here

  // TODO(david.turner@argondesign.com): Change all the encode strategy to
  // modify frame_params instead of cm or cpi.

  // Per-frame encode speed.  In theory this can vary, but things may have been
  // written assuming speed-level will not change within a sequence, so this
  // parameter should be used with caution.
  frame_params.speed = oxcf->speed;

  // Work out some encoding parameters specific to the pass:
  if (has_no_stats_stage(cpi) && cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
    av1_cyclic_refresh_update_parameters(cpi);
  } else if (is_stat_generation_stage(cpi)) {
    cpi->td.mb.e_mbd.lossless[0] = is_lossless_requested(&cpi->oxcf);
    const int kf_requested = (cm->current_frame.frame_number == 0 ||
                              (*frame_flags & FRAMEFLAGS_KEY));
    if (kf_requested && frame_update_type != OVERLAY_UPDATE &&
        frame_update_type != INTNL_OVERLAY_UPDATE) {
      frame_params.frame_type = KEY_FRAME;
    } else {
      frame_params.frame_type = INTER_FRAME;
    }
  } else if (is_stat_consumption_stage(cpi)) {
#if CONFIG_MISMATCH_DEBUG
    mismatch_move_frame_idx_w();
#endif
#if TXCOEFF_COST_TIMER
    cm->txcoeff_cost_timer = 0;
    cm->txcoeff_cost_count = 0;
#endif
  }

  if (!is_stat_generation_stage(cpi)) set_ext_overrides(cpi, &frame_params);

  // Shown keyframes and S frames refresh all reference buffers
  const int force_refresh_all =
      ((frame_params.frame_type == KEY_FRAME && frame_params.show_frame) ||
       frame_params.frame_type == S_FRAME) &&
      !frame_params.show_existing_frame;

  av1_configure_buffer_updates(cpi, &frame_params, frame_update_type,
                               force_refresh_all);

  if (!is_stat_generation_stage(cpi)) {
    if (!cpi->ext_refresh_frame_flags_pending)
      av1_get_ref_frames(cpi, frame_update_type, &cpi->ref_buffer_stack);

    // Work out which reference frame slots may be used.
    frame_params.ref_frame_flags = get_ref_frame_flags(cpi);

    frame_params.primary_ref_frame =
        choose_primary_ref_frame(cpi, &frame_params);
    frame_params.order_offset = get_order_offset(&cpi->gf_group, &frame_params);

    frame_params.refresh_frame_flags = av1_get_refresh_frame_flags(
        cpi, &frame_params, frame_update_type, &cpi->ref_buffer_stack);
  }

  // The way frame_params->remapped_ref_idx is setup is a placeholder.
  // Currently, reference buffer assignment is done by update_ref_frame_map()
  // which is called by high-level strategy AFTER encoding a frame.  It modifies
  // cm->remapped_ref_idx.  If you want to use an alternative method to
  // determine reference buffer assignment, just put your assignments into
  // frame_params->remapped_ref_idx here and they will be used when encoding
  // this frame.  If frame_params->remapped_ref_idx is setup independently of
  // cm->remapped_ref_idx then update_ref_frame_map() will have no effect.
  memcpy(frame_params.remapped_ref_idx, cm->remapped_ref_idx,
         REF_FRAMES * sizeof(*cm->remapped_ref_idx));

  cpi->td.mb.e_mbd.delta_qindex = 0;
#if ENABLE_KF_TPL
  if (oxcf->lag_in_frames > 0 && !is_stat_generation_stage(cpi) &&
      frame_params.frame_type == KEY_FRAME && frame_params.show_frame) {
    av1_configure_buffer_updates(cpi, &frame_params, frame_update_type, 0);
    av1_set_frame_size(cpi, cm->width, cm->height);
    av1_tpl_setup_stats(cpi, &frame_params, &frame_input);
  }
#endif  // ENABLE_KF_TPL

  if (!frame_params.show_existing_frame) {
    cm->using_qmatrix = cpi->oxcf.using_qm;
    cm->min_qmlevel = cpi->oxcf.qm_minlevel;
    cm->max_qmlevel = cpi->oxcf.qm_maxlevel;
    if (oxcf->lag_in_frames > 0 && !is_stat_generation_stage(cpi)) {
      if (cpi->gf_group.index == 1 && cpi->oxcf.enable_tpl_model) {
        av1_configure_buffer_updates(cpi, &frame_params, frame_update_type, 0);
        av1_set_frame_size(cpi, cm->width, cm->height);
        av1_tpl_setup_stats(cpi, &frame_params, &frame_input);
        assert(cpi->num_gf_group_show_frames == 1);
      }
    }
  }

#if TEMPORAL_FILTER_KEY_FRAME
  if (denoise_and_encode(cpi, dest, &frame_input, &frame_params, &frame_results,
                         &temporal_filtered) != AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }
#else   // !TEMPORAL_FILTER_KEY_FRAME
  cpi->pack_bitstream = 1;
  if (av1_encode(cpi, dest, &frame_input, &frame_params, &frame_results) !=
      AOM_CODEC_OK) {
    return AOM_CODEC_ERROR;
  }
#endif  // TEMPORAL_FILTER_KEY_FRAME
  if (!is_stat_generation_stage(cpi))
    cpi->num_gf_group_show_frames += frame_params.show_frame;

  if (!is_stat_generation_stage(cpi)) {
    // First pass doesn't modify reference buffer assignment or produce frame
    // flags
    update_frame_flags(cpi, frame_flags);
    int ref_map_index =
        av1_get_refresh_ref_frame_map(cm->current_frame.refresh_frame_flags);
    av1_update_ref_frame_map(cpi, frame_update_type, cm->show_existing_frame,
                             ref_map_index, &cpi->ref_buffer_stack);
  }

#if !CONFIG_REALTIME_ONLY
  if (!is_stat_generation_stage(cpi)) {
#if TXCOEFF_COST_TIMER
    cm->cum_txcoeff_cost_timer += cm->txcoeff_cost_timer;
    fprintf(stderr,
            "\ntxb coeff cost block number: %ld, frame time: %ld, cum time %ld "
            "in us\n",
            cm->txcoeff_cost_count, cm->txcoeff_cost_timer,
            cm->cum_txcoeff_cost_timer);
#endif
    av1_twopass_postencode_update(cpi);
  }
#endif  // !CONFIG_REALTIME_ONLY

  if (!is_stat_generation_stage(cpi)) {
    update_fb_of_context_type(cpi, &frame_params, cpi->fb_of_context_type);
    set_additional_frame_flags(cm, frame_flags);
    update_rc_counts(cpi);
  }

  // Unpack frame_results:
  *size = frame_results.size;

  // Leave a signal for a higher level caller about if this frame is droppable
  if (*size > 0) {
    cpi->droppable = is_frame_droppable(cpi);
  }

  return AOM_CODEC_OK;
}
