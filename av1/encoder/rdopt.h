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

#ifndef AV1_ENCODER_RDOPT_H_
#define AV1_ENCODER_RDOPT_H_

#include "av1/common/blockd.h"

#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TileInfo;
struct AV1_COMP;
struct macroblock;
struct RD_COST;

<<<<<<< HEAD   (005ff8 Merge "warped_motion: Fix ubsan warning for signed integer o)
int av1_cost_coeffs(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                    int block, int coeff_ctx, TX_SIZE tx_size,
                    const int16_t *scan, const int16_t *nb,
                    int use_fast_coef_costing);
void av1_rd_pick_intra_mode_sb(const struct AV1_COMP *cpi, struct macroblock *x,
                               struct RD_COST *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd);

unsigned int av1_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs);
#if CONFIG_AOM_HIGHBITDEPTH
unsigned int av1_high_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd);
#endif

void av1_rd_pick_inter_mode_sb(const struct AV1_COMP *cpi,
                               struct TileDataEnc *tile_data,
                               struct macroblock *x, int mi_row, int mi_col,
                               struct RD_COST *rd_cost,
#if CONFIG_SUPERTX
                               int *returnrate_nocoef,
#endif  // CONFIG_SUPERTX
                               BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                               int64_t best_rd_so_far);

void av1_rd_pick_inter_mode_sb_seg_skip(
    const struct AV1_COMP *cpi, struct TileDataEnc *tile_data,
    struct macroblock *x, struct RD_COST *rd_cost, BLOCK_SIZE bsize,
    PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

int av1_internal_image_edge(const struct AV1_COMP *cpi);
int av1_active_h_edge(const struct AV1_COMP *cpi, int mi_row, int mi_step);
int av1_active_v_edge(const struct AV1_COMP *cpi, int mi_col, int mi_step);
int av1_active_edge_sb(const struct AV1_COMP *cpi, int mi_row, int mi_col);

void av1_rd_pick_inter_mode_sub8x8(const struct AV1_COMP *cpi,
                                   struct TileDataEnc *tile_data,
                                   struct macroblock *x, int mi_row, int mi_col,
                                   struct RD_COST *rd_cost,
#if CONFIG_SUPERTX
                                   int *returnrate_nocoef,
#endif  // CONFIG_SUPERTX
                                   BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                                   int64_t best_rd_so_far);

#if CONFIG_SUPERTX
#if CONFIG_VAR_TX
void av1_tx_block_rd_b(const AV1_COMP *cpi, MACROBLOCK *x, TX_SIZE tx_size,
                       int blk_row, int blk_col, int plane, int block,
                       int plane_bsize, int coeff_ctx, int *rate, int64_t *dist,
                       int64_t *bsse, int *skip);
#endif

void av1_txfm_rd_in_plane_supertx(MACROBLOCK *x, const AV1_COMP *cpi, int *rate,
                                  int64_t *distortion, int *skippable,
                                  int64_t *sse, int64_t ref_best_rd, int plane,
                                  BLOCK_SIZE bsize, TX_SIZE tx_size,
                                  int use_fast_coef_casting);
#endif  // CONFIG_SUPERTX
=======
void av1_rd_pick_intra_mode_sb(const struct AV1_COMP *cpi, struct macroblock *x,
                               struct RD_COST *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd);

unsigned int av1_get_sby_perpixel_variance(const struct AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs);
#if CONFIG_AOM_HIGHBITDEPTH
unsigned int av1_high_get_sby_perpixel_variance(const struct AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd);
#endif

void av1_rd_pick_inter_mode_sb(const struct AV1_COMP *cpi,
                               struct TileDataEnc *tile_data,
                               struct macroblock *x, int mi_row, int mi_col,
                               struct RD_COST *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

void av1_rd_pick_inter_mode_sb_seg_skip(
    const struct AV1_COMP *cpi, struct TileDataEnc *tile_data,
    struct macroblock *x, struct RD_COST *rd_cost, BLOCK_SIZE bsize,
    PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

int av1_internal_image_edge(const struct AV1_COMP *cpi);
int av1_active_h_edge(const struct AV1_COMP *cpi, int mi_row, int mi_step);
int av1_active_v_edge(const struct AV1_COMP *cpi, int mi_col, int mi_step);
int av1_active_edge_sb(const struct AV1_COMP *cpi, int mi_row, int mi_col);

void av1_rd_pick_inter_mode_sub8x8(const struct AV1_COMP *cpi,
                                   struct TileDataEnc *tile_data,
                                   struct macroblock *x, int mi_row, int mi_col,
                                   struct RD_COST *rd_cost, BLOCK_SIZE bsize,
                                   PICK_MODE_CONTEXT *ctx,
                                   int64_t best_rd_so_far);
>>>>>>> BRANCH (5bf37c Use --enable-daala_ec by default.)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_RDOPT_H_
