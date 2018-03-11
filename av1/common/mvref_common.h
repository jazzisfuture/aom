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
#ifndef AV1_COMMON_MVREF_COMMON_H_
#define AV1_COMMON_MVREF_COMMON_H_

#include "av1/common/onyxc_int.h"
#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MVREF_ROWS 3
#define MVREF_COLS 3

// Set the upper limit of the motion vector component magnitude.
// This would make a motion vector fit in 26 bits. Plus 3 bits for the
// reference frame index. A tuple of motion vector can hence be stored within
// 32 bit range for efficient load/store operations.
#define REFMVS_LIMIT (1 << 12)

typedef struct position {
  int row;
  int col;
} POSITION;

// clamp_mv_ref
#define MV_BORDER (16 << 3)  // Allow 16 pels in 1/8th pel units

#if CONFIG_EXPLICIT_ORDER_HINT
static INLINE int get_relative_dist_b(int bits, int a, int b) {
  // IMDAD: CAN bits EVER ACTUALLY BE 0?
  // IMDAD: we find that if bits == 1, then sometimes b > (1 << bits) = 2.
  assert(a >= 0 && a < (1 << bits));
  assert(b >= 0 && b < (1 << bits));
  if (bits == 0) {
    return 0;
  } else {
    int diff = a - b;
    int m = 1 << (bits - 1);
    diff = (diff & (m - 1)) - (diff & m);
    return diff;
  }
}

static INLINE int get_relative_dist(const AV1_COMMON *cm, int a, int b) {
  return get_relative_dist_b(cm->seq_params.order_hint_bits, a, b);
}
#endif

// Get the number of frames between the current frame and a reference frame
static INLINE int get_ref_frame_dist(const AV1_COMMON *cm,
                                     MV_REFERENCE_FRAME ref) {
  // get the offset between the key frame and the current frame
  const int cur_frame_offset = cm->frame_offset;
  // get the offset between the key frame and the reference frame
  const int ref_buf_idx = cm->frame_refs[ref - LAST_FRAME].idx;
  if (ref_buf_idx == INVALID_IDX) return INT_MAX;
  const int ref_frame_offset =
      cm->buffer_pool->frame_bufs[ref_buf_idx].cur_frame_offset;
#if CONFIG_EXPLICIT_ORDER_HINT
  return get_relative_dist(cm, cur_frame_offset, ref_frame_offset);
#else
  return cur_frame_offset - ref_frame_offset;
#endif
}

static INLINE void clamp_mv_ref(MV *mv, int bw, int bh, const MACROBLOCKD *xd) {
  clamp_mv(mv, xd->mb_to_left_edge - bw * 8 - MV_BORDER,
           xd->mb_to_right_edge + bw * 8 + MV_BORDER,
           xd->mb_to_top_edge - bh * 8 - MV_BORDER,
           xd->mb_to_bottom_edge + bh * 8 + MV_BORDER);
}

// This function returns either the appropriate sub block or block's mv
// on whether the block_size < 8x8 and we have check_sub_blocks set.
static INLINE int_mv get_sub_block_mv(const MODE_INFO *candidate, int which_mv,
                                      int search_col) {
  (void)search_col;
  return candidate->mbmi.mv[which_mv];
}

static INLINE int_mv get_sub_block_pred_mv(const MODE_INFO *candidate,
                                           int which_mv, int search_col) {
  (void)search_col;
  return candidate->mbmi.mv[which_mv];
}

// Performs mv sign inversion if indicated by the reference frame combination.
static INLINE int_mv scale_mv(const MB_MODE_INFO *mbmi, int ref,
                              const MV_REFERENCE_FRAME this_ref_frame,
                              const int *ref_sign_bias) {
  int_mv mv = mbmi->mv[ref];
  if (ref_sign_bias[mbmi->ref_frame[ref]] != ref_sign_bias[this_ref_frame]) {
    mv.as_mv.row *= -1;
    mv.as_mv.col *= -1;
  }
  return mv;
}

// Checks that the given mi_row, mi_col and search point
// are inside the borders of the tile.
static INLINE int is_inside(const TileInfo *const tile, int mi_col, int mi_row,
                            int mi_rows, const AV1_COMMON *cm,
                            const POSITION *mi_pos) {
#if CONFIG_DEPENDENT_HORZTILES
  const int dependent_horz_tile_flag = cm->dependent_horz_tiles;
#else
  const int dependent_horz_tile_flag = 0;
  (void)cm;
#endif
  if (dependent_horz_tile_flag && !tile->tg_horz_boundary) {
    return !(mi_row + mi_pos->row < 0 ||
             mi_col + mi_pos->col < tile->mi_col_start ||
             mi_row + mi_pos->row >= mi_rows ||
             mi_col + mi_pos->col >= tile->mi_col_end);
  } else {
    return !(mi_row + mi_pos->row < tile->mi_row_start ||
             mi_col + mi_pos->col < tile->mi_col_start ||
             mi_row + mi_pos->row >= tile->mi_row_end ||
             mi_col + mi_pos->col >= tile->mi_col_end);
  }
}

static INLINE int find_valid_row_offset(const TileInfo *const tile, int mi_row,
                                        int mi_rows, const AV1_COMMON *cm,
                                        int row_offset) {
#if CONFIG_DEPENDENT_HORZTILES
  const int dependent_horz_tile_flag = cm->dependent_horz_tiles;
#else
  const int dependent_horz_tile_flag = 0;
  (void)cm;
#endif
  if (dependent_horz_tile_flag && !tile->tg_horz_boundary)
    return clamp(row_offset, -mi_row, mi_rows - mi_row - 1);
  else
    return clamp(row_offset, tile->mi_row_start - mi_row,
                 tile->mi_row_end - mi_row - 1);
}

static INLINE int find_valid_col_offset(const TileInfo *const tile, int mi_col,
                                        int col_offset) {
  return clamp(col_offset, tile->mi_col_start - mi_col,
               tile->mi_col_end - mi_col - 1);
}

static INLINE void lower_mv_precision(MV *mv, int allow_hp
#if CONFIG_AMVR
                                      ,
                                      int is_integer
#endif
) {
#if CONFIG_AMVR
  if (is_integer) {
    integer_mv_precision(mv);
  } else {
#endif
    if (!allow_hp) {
      if (mv->row & 1) mv->row += (mv->row > 0 ? -1 : 1);
      if (mv->col & 1) mv->col += (mv->col > 0 ? -1 : 1);
    }
#if CONFIG_AMVR
  }
#endif
}

static INLINE uint8_t av1_get_pred_diff_ctx(const int_mv pred_mv,
                                            const int_mv this_mv) {
  if (abs(this_mv.as_mv.row - pred_mv.as_mv.row) <= 4 &&
      abs(this_mv.as_mv.col - pred_mv.as_mv.col) <= 4)
    return 2;
  else
    return 1;
}

static INLINE int av1_nmv_ctx(const uint8_t ref_mv_count,
                              const CANDIDATE_MV *ref_mv_stack, int ref,
                              int ref_mv_idx) {
  return 0;

  if (ref_mv_idx < ref_mv_count &&
      ref_mv_stack[ref_mv_idx].weight >= REF_CAT_LEVEL)
    return ref_mv_stack[ref_mv_idx].pred_diff[ref];

  return 0;
}

static INLINE int8_t get_uni_comp_ref_idx(const MV_REFERENCE_FRAME *const rf) {
  // Single ref pred
  if (rf[1] <= INTRA_FRAME) return -1;

  // Bi-directional comp ref pred
  if ((rf[0] < BWDREF_FRAME) && (rf[1] >= BWDREF_FRAME)) return -1;

  for (int8_t ref_idx = 0; ref_idx < TOTAL_UNIDIR_COMP_REFS; ++ref_idx) {
    if (rf[0] == comp_ref0(ref_idx) && rf[1] == comp_ref1(ref_idx))
      return ref_idx;
  }
  return -1;
}

static INLINE int8_t av1_ref_frame_type(const MV_REFERENCE_FRAME *const rf) {
  if (rf[1] > INTRA_FRAME) {
    const int8_t uni_comp_ref_idx = get_uni_comp_ref_idx(rf);
    if (uni_comp_ref_idx >= 0) {
      assert((TOTAL_REFS_PER_FRAME + FWD_REFS * BWD_REFS + uni_comp_ref_idx) <
             MODE_CTX_REF_FRAMES);
      return TOTAL_REFS_PER_FRAME + FWD_REFS * BWD_REFS + uni_comp_ref_idx;
    } else {
      return TOTAL_REFS_PER_FRAME + FWD_RF_OFFSET(rf[0]) +
             BWD_RF_OFFSET(rf[1]) * FWD_REFS;
    }
  }

  return rf[0];
}

// clang-format off
static MV_REFERENCE_FRAME ref_frame_map[TOTAL_COMP_REFS][2] = {
  { LAST_FRAME, BWDREF_FRAME },  { LAST2_FRAME, BWDREF_FRAME },
  { LAST3_FRAME, BWDREF_FRAME }, { GOLDEN_FRAME, BWDREF_FRAME },

  { LAST_FRAME, ALTREF2_FRAME },  { LAST2_FRAME, ALTREF2_FRAME },
  { LAST3_FRAME, ALTREF2_FRAME }, { GOLDEN_FRAME, ALTREF2_FRAME },

  { LAST_FRAME, ALTREF_FRAME },  { LAST2_FRAME, ALTREF_FRAME },
  { LAST3_FRAME, ALTREF_FRAME }, { GOLDEN_FRAME, ALTREF_FRAME },

  { LAST_FRAME, LAST2_FRAME }, { LAST_FRAME, LAST3_FRAME },
  { LAST_FRAME, GOLDEN_FRAME }, { BWDREF_FRAME, ALTREF_FRAME },

  // NOTE: Following reference frame pairs are not supported to be explicitly
  //       signalled, but they are possibly chosen by the use of skip_mode,
  //       which may use the most recent one-sided reference frame pair.
  { LAST2_FRAME, LAST3_FRAME }, { LAST2_FRAME, GOLDEN_FRAME },
  { LAST3_FRAME, GOLDEN_FRAME }, {BWDREF_FRAME, ALTREF2_FRAME},
  { ALTREF2_FRAME, ALTREF_FRAME }
};
// clang-format on

static INLINE void av1_set_ref_frame(MV_REFERENCE_FRAME *rf,
                                     int8_t ref_frame_type) {
  if (ref_frame_type >= TOTAL_REFS_PER_FRAME) {
    rf[0] = ref_frame_map[ref_frame_type - TOTAL_REFS_PER_FRAME][0];
    rf[1] = ref_frame_map[ref_frame_type - TOTAL_REFS_PER_FRAME][1];
  } else {
    rf[0] = ref_frame_type;
    rf[1] = NONE_FRAME;
    assert(ref_frame_type > NONE_FRAME);
  }
}

static INLINE int16_t av1_mode_context_analyzer(
    const int16_t *const mode_context, const MV_REFERENCE_FRAME *const rf) {
  const int8_t ref_frame = av1_ref_frame_type(rf);

  if (rf[1] <= INTRA_FRAME) return mode_context[ref_frame];

  const int16_t newmv_ctx = mode_context[ref_frame] & NEWMV_CTX_MASK;
  const int16_t refmv_ctx =
      (mode_context[ref_frame] >> REFMV_OFFSET) & REFMV_CTX_MASK;
  const int16_t comp_ctx = (refmv_ctx >> 1) * COMP_NEWMV_CTXS +
                           AOMMIN(newmv_ctx, COMP_NEWMV_CTXS - 1);
  return comp_ctx;
}

static INLINE uint8_t av1_drl_ctx(const CANDIDATE_MV *ref_mv_stack,
                                  int ref_idx) {
  if (ref_mv_stack[ref_idx].weight >= REF_CAT_LEVEL &&
      ref_mv_stack[ref_idx + 1].weight >= REF_CAT_LEVEL)
    return 0;

  if (ref_mv_stack[ref_idx].weight >= REF_CAT_LEVEL &&
      ref_mv_stack[ref_idx + 1].weight < REF_CAT_LEVEL)
    return 1;

  if (ref_mv_stack[ref_idx].weight < REF_CAT_LEVEL &&
      ref_mv_stack[ref_idx + 1].weight < REF_CAT_LEVEL)
    return 2;

  return 0;
}

static INLINE int av1_is_compound_reference_allowed(const AV1_COMMON *cm) {
  if (frame_is_intra_only(cm)) return 0;
  // Check whether two different reference frames exist.
  int ref, ref_offset0;
  int is_comp_allowed = 0;

  for (ref = 0; ref < INTER_REFS_PER_FRAME; ++ref) {
    const int buf_idx = cm->frame_refs[ref].idx;
    if (buf_idx == INVALID_IDX) continue;
    ref_offset0 = cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset;
    ref++;
    break;
  }

  for (; ref < INTER_REFS_PER_FRAME; ++ref) {
    const int buf_idx = cm->frame_refs[ref].idx;
    if (buf_idx == INVALID_IDX) continue;
    const int ref_offset =
        cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset;
    if (ref_offset != ref_offset0) {
      is_comp_allowed = 1;
      break;
    }
  }

  return is_comp_allowed;
}

static INLINE int av1_refs_are_one_sided(const AV1_COMMON *cm) {
  assert(!frame_is_intra_only(cm));

  int one_sided_refs = 1;
  for (int ref = 0; ref < INTER_REFS_PER_FRAME; ++ref) {
    const int buf_idx = cm->frame_refs[ref].idx;
    if (buf_idx == INVALID_IDX) continue;

    const int ref_offset =
        cm->buffer_pool->frame_bufs[buf_idx].cur_frame_offset;
#if CONFIG_EXPLICIT_ORDER_HINT
    if (get_relative_dist(cm, ref_offset, (int)cm->frame_offset) > 0) {
#else
    if (ref_offset > (int)cm->frame_offset) {
#endif
      one_sided_refs = 0;  // bwd reference
      break;
    }
  }
  return one_sided_refs;
}

void av1_setup_frame_buf_refs(AV1_COMMON *cm);
void av1_setup_frame_sign_bias(AV1_COMMON *cm);
void av1_setup_skip_mode_allowed(AV1_COMMON *cm);
void av1_setup_motion_field(AV1_COMMON *cm);

#if CONFIG_FRAME_REFS_SIGNALING
void av1_set_frame_refs(AV1_COMMON *const cm, int lst_map_idx, int gld_map_idx);
#endif  // CONFIG_FRAME_REFS_SIGNALING

static INLINE void av1_collect_neighbors_ref_counts(MACROBLOCKD *const xd) {
  av1_zero(xd->neighbors_ref_counts);

  uint8_t *const ref_counts = xd->neighbors_ref_counts;

  const MB_MODE_INFO *const above_mbmi = xd->above_mbmi;
  const MB_MODE_INFO *const left_mbmi = xd->left_mbmi;
  const int above_in_image = xd->up_available;
  const int left_in_image = xd->left_available;

  // Above neighbor
  if (above_in_image && is_inter_block(above_mbmi)) {
    ref_counts[above_mbmi->ref_frame[0]]++;
    if (has_second_ref(above_mbmi)) {
      ref_counts[above_mbmi->ref_frame[1]]++;
    }
  }

  // Left neighbor
  if (left_in_image && is_inter_block(left_mbmi)) {
    ref_counts[left_mbmi->ref_frame[0]]++;
    if (has_second_ref(left_mbmi)) {
      ref_counts[left_mbmi->ref_frame[1]]++;
    }
  }
}

void av1_copy_frame_mvs(const AV1_COMMON *const cm, MODE_INFO *mi, int mi_row,
                        int mi_col, int x_mis, int y_mis);

typedef void (*find_mv_refs_sync)(void *const data, int mi_row);
void av1_find_mv_refs(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                      MODE_INFO *mi, MV_REFERENCE_FRAME ref_frame,
                      uint8_t ref_mv_count[MODE_CTX_REF_FRAMES],
                      CANDIDATE_MV ref_mv_stack[][MAX_REF_MV_STACK_SIZE],
                      int16_t *compound_mode_context,
                      int_mv mv_ref_list[][MAX_MV_REF_CANDIDATES], int mi_row,
                      int mi_col, find_mv_refs_sync sync, void *const data,
                      int16_t *mode_context, int compound_search);

// check a list of motion vectors by sad score using a number rows of pixels
// above and a number cols of pixels in the left to select the one with best
// score to use as ref motion vector
#if CONFIG_AMVR
void av1_find_best_ref_mvs(int allow_hp, int_mv *mvlist, int_mv *nearest_mv,
                           int_mv *near_mv, int is_integer);
#else
void av1_find_best_ref_mvs(int allow_hp, int_mv *mvlist, int_mv *nearest_mv,
                           int_mv *near_mv);
#endif

#if CONFIG_EXT_WARPED_MOTION
int selectSamples(MV *mv, int *pts, int *pts_inref, int len, BLOCK_SIZE bsize);
#endif  // CONFIG_EXT_WARPED_MOTION
int findSamples(const AV1_COMMON *cm, MACROBLOCKD *xd, int mi_row, int mi_col,
                int *pts, int *pts_inref);

#define INTRABC_DELAY_PIXELS 256  //  Delay of 256 pixels
#define INTRABC_DELAY_SB64 (INTRABC_DELAY_PIXELS / 64)
#define USE_WAVE_FRONT 1  // Use only top left area of frame for reference.

static INLINE void av1_find_ref_dv(int_mv *ref_dv, const TileInfo *const tile,
                                   int mib_size, int mi_row, int mi_col) {
  (void)mi_col;
  if (mi_row - mib_size < tile->mi_row_start) {
    ref_dv->as_mv.row = 0;
    ref_dv->as_mv.col = -MI_SIZE * mib_size - INTRABC_DELAY_PIXELS;
  } else {
    ref_dv->as_mv.row = -MI_SIZE * mib_size;
    ref_dv->as_mv.col = 0;
  }
  ref_dv->as_mv.row *= 8;
  ref_dv->as_mv.col *= 8;
}

static INLINE int av1_is_dv_valid(const MV dv, const TileInfo *const tile,
                                  int mi_row, int mi_col, BLOCK_SIZE bsize,
                                  int mib_size_log2) {
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int SCALE_PX_TO_MV = 8;
  // Disallow subpixel for now
  // SUBPEL_MASK is not the correct scale
  if (((dv.row & (SCALE_PX_TO_MV - 1)) || (dv.col & (SCALE_PX_TO_MV - 1))))
    return 0;

  // Is the source top-left inside the current tile?
  const int src_top_edge = mi_row * MI_SIZE * SCALE_PX_TO_MV + dv.row;
  const int tile_top_edge = tile->mi_row_start * MI_SIZE * SCALE_PX_TO_MV;
  if (src_top_edge < tile_top_edge) return 0;
  const int src_left_edge = mi_col * MI_SIZE * SCALE_PX_TO_MV + dv.col;
  const int tile_left_edge = tile->mi_col_start * MI_SIZE * SCALE_PX_TO_MV;
  if (src_left_edge < tile_left_edge) return 0;
  // Is the bottom right inside the current tile?
  const int src_bottom_edge = (mi_row * MI_SIZE + bh) * SCALE_PX_TO_MV + dv.row;
  const int tile_bottom_edge = tile->mi_row_end * MI_SIZE * SCALE_PX_TO_MV;
  if (src_bottom_edge > tile_bottom_edge) return 0;
  const int src_right_edge = (mi_col * MI_SIZE + bw) * SCALE_PX_TO_MV + dv.col;
  const int tile_right_edge = tile->mi_col_end * MI_SIZE * SCALE_PX_TO_MV;
  if (src_right_edge > tile_right_edge) return 0;

  // Is the bottom right within an already coded SB? Also consider additional
  // constraints to facilitate HW decoder.
  const int max_mib_size = 1 << mib_size_log2;
  const int active_sb_row = mi_row >> mib_size_log2;
  const int active_sb64_col = (mi_col * MI_SIZE) >> 6;
  const int sb_size = max_mib_size * MI_SIZE;
  const int src_sb_row = ((src_bottom_edge >> 3) - 1) / sb_size;
  const int src_sb64_col = ((src_right_edge >> 3) - 1) >> 6;
  const int total_sb64_per_row =
      ((tile->mi_col_end - tile->mi_col_start - 1) >> 4) + 1;
  const int active_sb64 = active_sb_row * total_sb64_per_row + active_sb64_col;
  const int src_sb64 = src_sb_row * total_sb64_per_row + src_sb64_col;
  if (src_sb64 >= active_sb64 - INTRABC_DELAY_SB64) return 0;

#if USE_WAVE_FRONT
  const int gradient = 1 + INTRABC_DELAY_SB64 + (sb_size > 64);
  const int wf_offset = gradient * (active_sb_row - src_sb_row);
  if (src_sb_row > active_sb_row ||
      src_sb64_col >= active_sb64_col - INTRABC_DELAY_SB64 + wf_offset)
    return 0;
#endif

  return 1;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_MVREF_COMMON_H_
