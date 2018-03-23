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

#ifndef AV1_COMMON_PRED_COMMON_H_
#define AV1_COMMON_PRED_COMMON_H_

#include "av1/common/blockd.h"
#if CONFIG_EXPLICIT_ORDER_HINT
#include "av1/common/mvref_common.h"
#endif
#include "av1/common/onyxc_int.h"
#include "aom_dsp/aom_dsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

static INLINE int get_segment_id(const AV1_COMMON *const cm,
                                 const uint8_t *segment_ids, BLOCK_SIZE bsize,
                                 int mi_row, int mi_col) {
  const int mi_offset = mi_row * cm->mi_cols + mi_col;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int xmis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int ymis = AOMMIN(cm->mi_rows - mi_row, bh);
  int x, y, segment_id = MAX_SEGMENTS;

  for (y = 0; y < ymis; ++y)
    for (x = 0; x < xmis; ++x)
      segment_id =
          AOMMIN(segment_id, segment_ids[mi_offset + y * cm->mi_cols + x]);

  assert(segment_id >= 0 && segment_id < MAX_SEGMENTS);
  return segment_id;
}

#if CONFIG_SPATIAL_SEGMENTATION
static INLINE int av1_get_spatial_seg_pred(const AV1_COMMON *const cm,
                                           const MACROBLOCKD *const xd,
                                           int mi_row, int mi_col,
                                           int *cdf_index) {
  int prev_ul = -1;  // top left segment_id
  int prev_l = -1;   // left segment_id
  int prev_u = -1;   // top segment_id
  if ((xd->up_available) && (xd->left_available)) {
    prev_ul = get_segment_id(cm, cm->current_frame_seg_map, BLOCK_4X4,
                             mi_row - 1, mi_col - 1);
  }
  if (xd->up_available) {
    prev_u = get_segment_id(cm, cm->current_frame_seg_map, BLOCK_4X4,
                            mi_row - 1, mi_col - 0);
  }
  if (xd->left_available) {
    prev_l = get_segment_id(cm, cm->current_frame_seg_map, BLOCK_4X4,
                            mi_row - 0, mi_col - 1);
  }

  // Pick CDF index based on number of matching/out-of-bounds segment IDs.
  if (prev_ul < 0 || prev_u < 0 || prev_l < 0) /* Edge case */
    *cdf_index = 0;
  else if ((prev_ul == prev_u) && (prev_ul == prev_l))
    *cdf_index = 2;
  else if ((prev_ul == prev_u) || (prev_ul == prev_l) || (prev_u == prev_l))
    *cdf_index = 1;
  else
    *cdf_index = 0;

  // If 2 or more are identical returns that as predictor, otherwise prev_l.
  if (prev_u == -1)  // edge case
    return prev_l == -1 ? 0 : prev_l;
  if (prev_l == -1)  // edge case
    return prev_u;
  return (prev_ul == prev_u) ? prev_u : prev_l;
}
#endif  // CONFIG_SPATIAL_SEGMENTATION

static INLINE int av1_get_pred_context_seg_id(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_sip =
      (above_mi != NULL) ? above_mi->mbmi.seg_id_predicted : 0;
  const int left_sip = (left_mi != NULL) ? left_mi->mbmi.seg_id_predicted : 0;

  return above_sip + left_sip;
}

static INLINE int get_comp_group0_context(const AV1_COMMON *cm,
                                          const MACROBLOCKD *xd) {
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int bck_idx = cm->frame_refs[mbmi->ref_frame[0] - LAST_FRAME].idx;
  int fwd_idx = cm->frame_refs[mbmi->ref_frame[1] - LAST_FRAME].idx;
  int bck_frame_index = 0, fwd_frame_index = 0;
  int cur_frame_index = cm->cur_frame->cur_frame_offset;
  assert(!mbmi->comp_group_idx);

  if (bck_idx >= 0)
    bck_frame_index = cm->buffer_pool->frame_bufs[bck_idx].cur_frame_offset;

  if (fwd_idx >= 0)
    fwd_frame_index = cm->buffer_pool->frame_bufs[fwd_idx].cur_frame_offset;
#if CONFIG_EXPLICIT_ORDER_HINT
  int fwd = abs(get_relative_dist(cm, fwd_frame_index, cur_frame_index));
  int bck = abs(get_relative_dist(cm, cur_frame_index, bck_frame_index));
#else
  int fwd = abs(fwd_frame_index - cur_frame_index);
  int bck = abs(cur_frame_index - bck_frame_index);
#endif

  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;

  int above_ctx = 0, left_ctx = 0;
  const int offset = (fwd == bck);

  if (above_mi) {
    const MB_MODE_INFO *above_mbmi = &above_mi->mbmi;
    if (has_second_ref(above_mbmi))
      above_ctx = (above_mbmi->interinter_compound_type == COMPOUND_AVERAGE);
    else if (above_mbmi->ref_frame[0] == ALTREF_FRAME)
      above_ctx = 1;
  }

  if (left_mi) {
    const MB_MODE_INFO *left_mbmi = &left_mi->mbmi;
    if (has_second_ref(left_mbmi))
      left_ctx = (left_mbmi->interinter_compound_type == COMPOUND_AVERAGE);
    else if (left_mbmi->ref_frame[0] == ALTREF_FRAME)
      left_ctx = 1;
  }

  return above_ctx + left_ctx + 3 * offset;
}

static INLINE int get_comp_group_idx_context(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  int above_ctx = 0, left_ctx = 0;

  if (above_mi) {
    const MB_MODE_INFO *above_mbmi = &above_mi->mbmi;
    if (has_second_ref(above_mbmi))
      above_ctx = above_mbmi->comp_group_idx;
    else if (above_mbmi->ref_frame[0] == ALTREF_FRAME)
      above_ctx = 3;
  }
  if (left_mi) {
    const MB_MODE_INFO *left_mbmi = &left_mi->mbmi;
    if (has_second_ref(left_mbmi))
      left_ctx = left_mbmi->comp_group_idx;
    else if (left_mbmi->ref_frame[0] == ALTREF_FRAME)
      left_ctx = 3;
  }

  return above_ctx + left_ctx;
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_seg_id(
    struct segmentation_probs *segp, const MACROBLOCKD *xd) {
  return segp->pred_cdf[av1_get_pred_context_seg_id(xd)];
}

static INLINE int av1_get_skip_mode_context(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_skip_mode = above_mi ? above_mi->mbmi.skip_mode : 0;
  const int left_skip_mode = left_mi ? left_mi->mbmi.skip_mode : 0;
  return above_skip_mode + left_skip_mode;
}

static INLINE int av1_get_skip_context(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  const int above_skip = (above_mi != NULL) ? above_mi->mbmi.skip : 0;
  const int left_skip = (left_mi != NULL) ? left_mi->mbmi.skip : 0;
  return above_skip + left_skip;
}

int av1_get_pred_context_switchable_interp(const MACROBLOCKD *xd, int dir);

// Get a list of palette base colors that are used in the above and left blocks,
// referred to as "color cache". The return value is the number of colors in the
// cache (<= 2 * PALETTE_MAX_SIZE). The color values are stored in "cache"
// in ascending order.
int av1_get_palette_cache(const MACROBLOCKD *const xd, int plane,
                          uint16_t *cache);

static INLINE int av1_get_palette_bsize_ctx(BLOCK_SIZE bsize) {
  return num_pels_log2_lookup[bsize] - num_pels_log2_lookup[BLOCK_4X4];
}

static INLINE int av1_get_palette_mode_ctx(const MACROBLOCKD *xd) {
  const MODE_INFO *const above_mi = xd->above_mi;
  const MODE_INFO *const left_mi = xd->left_mi;
  int ctx = 0;
  if (above_mi) ctx += (above_mi->mbmi.palette_mode_info.palette_size[0] > 0);
  if (left_mi) ctx += (left_mi->mbmi.palette_mode_info.palette_size[0] > 0);
  return ctx;
}

int av1_get_intra_inter_context(const MACROBLOCKD *xd);

int av1_get_reference_mode_context(const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_reference_mode_cdf(const MACROBLOCKD *xd) {
  return xd->tile_ctx->comp_inter_cdf[av1_get_reference_mode_context(xd)];
}

int av1_get_comp_reference_type_context(const MACROBLOCKD *xd);

// == Uni-directional contexts ==

int av1_get_pred_context_uni_comp_ref_p(const MACROBLOCKD *xd);

int av1_get_pred_context_uni_comp_ref_p1(const MACROBLOCKD *xd);

int av1_get_pred_context_uni_comp_ref_p2(const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_comp_reference_type_cdf(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_comp_reference_type_context(xd);
  return xd->tile_ctx->comp_ref_type_cdf[pred_context];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_uni_comp_ref_p(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p(xd);
  return xd->tile_ctx->uni_comp_ref_cdf[pred_context][0];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_uni_comp_ref_p1(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p1(xd);
  return xd->tile_ctx->uni_comp_ref_cdf[pred_context][1];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_uni_comp_ref_p2(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_uni_comp_ref_p2(xd);
  return xd->tile_ctx->uni_comp_ref_cdf[pred_context][2];
}

// == Bi-directional contexts ==

int av1_get_pred_context_comp_ref_p(const MACROBLOCKD *xd);

int av1_get_pred_context_comp_ref_p1(const MACROBLOCKD *xd);

int av1_get_pred_context_comp_ref_p2(const MACROBLOCKD *xd);

int av1_get_pred_context_comp_bwdref_p(const MACROBLOCKD *xd);

int av1_get_pred_context_comp_bwdref_p1(const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_ref_p(const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p(xd);
  return xd->tile_ctx->comp_ref_cdf[pred_context][0];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_ref_p1(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p1(xd);
  return xd->tile_ctx->comp_ref_cdf[pred_context][1];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_ref_p2(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_ref_p2(xd);
  return xd->tile_ctx->comp_ref_cdf[pred_context][2];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_bwdref_p(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_bwdref_p(xd);
  return xd->tile_ctx->comp_bwdref_cdf[pred_context][0];
}

static INLINE aom_cdf_prob *av1_get_pred_cdf_comp_bwdref_p1(
    const MACROBLOCKD *xd) {
  const int pred_context = av1_get_pred_context_comp_bwdref_p1(xd);
  return xd->tile_ctx->comp_bwdref_cdf[pred_context][1];
}

// == Single contexts ==

int av1_get_pred_context_single_ref_p1(const MACROBLOCKD *xd);

int av1_get_pred_context_single_ref_p2(const MACROBLOCKD *xd);

int av1_get_pred_context_single_ref_p3(const MACROBLOCKD *xd);

int av1_get_pred_context_single_ref_p4(const MACROBLOCKD *xd);

int av1_get_pred_context_single_ref_p5(const MACROBLOCKD *xd);

int av1_get_pred_context_single_ref_p6(const MACROBLOCKD *xd);

static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p1(
    const MACROBLOCKD *xd) {
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p1(xd)][0];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p2(
    const MACROBLOCKD *xd) {
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p2(xd)][1];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p3(
    const MACROBLOCKD *xd) {
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p3(xd)][2];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p4(
    const MACROBLOCKD *xd) {
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p4(xd)][3];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p5(
    const MACROBLOCKD *xd) {
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p5(xd)][4];
}
static INLINE aom_cdf_prob *av1_get_pred_cdf_single_ref_p6(
    const MACROBLOCKD *xd) {
  return xd->tile_ctx
      ->single_ref_cdf[av1_get_pred_context_single_ref_p6(xd)][5];
}

// Returns a context number for the given MB prediction signal
// The mode info data structure has a one element border above and to the
// left of the entries corresponding to real blocks.
// The prediction flags in these dummy entries are initialized to 0.
static INLINE int get_tx_size_context(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const TX_SIZE max_tx_size = max_txsize_rect_lookup[mbmi->sb_type];
  const int max_tx_wide = tx_size_wide[max_tx_size];
  const int max_tx_high = tx_size_high[max_tx_size];
  const int has_above = xd->up_available;
  const int has_left = xd->left_available;

  int above = xd->above_txfm_context[0] >= max_tx_wide;
  int left = xd->left_txfm_context[0] >= max_tx_high;

  if (has_above)
    if (is_inter_block(above_mbmi))
      above = block_size_wide[above_mbmi->sb_type] >= max_tx_wide;

  if (has_left)
    if (is_inter_block(left_mbmi))
      left = block_size_high[left_mbmi->sb_type] >= max_tx_high;

  if (has_above && has_left)
    return (above + left);
  else if (has_above)
    return above;
  else if (has_left)
    return left;
  else
    return 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_PRED_COMMON_H_
