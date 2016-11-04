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

#ifndef AV1_ENCODER_ENCODEMB_H_
#define AV1_ENCODER_ENCODEMB_H_

#include "./aom_config.h"
<<<<<<< HEAD   (005ff8 Merge "warped_motion: Fix ubsan warning for signed integer o)
#include "av1/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

struct optimize_ctx {
  ENTROPY_CONTEXT ta[MAX_MB_PLANE][2 * MAX_MIB_SIZE];
  ENTROPY_CONTEXT tl[MAX_MB_PLANE][2 * MAX_MIB_SIZE];
};

struct encode_b_args {
  AV1_COMMON *cm;
  MACROBLOCK *x;
  struct optimize_ctx *ctx;
  int8_t *skip;
  ENTROPY_CONTEXT *ta;
  ENTROPY_CONTEXT *tl;
  int8_t enable_optimize_b;
};

typedef enum AV1_XFORM_QUANT {
  AV1_XFORM_QUANT_FP = 0,
  AV1_XFORM_QUANT_B = 1,
  AV1_XFORM_QUANT_DC = 2,
  AV1_XFORM_QUANT_SKIP_QUANT = 3,
  AV1_XFORM_QUANT_LAST = 4
} AV1_XFORM_QUANT;

void av1_encode_sb(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
#if CONFIG_SUPERTX
void av1_encode_sb_supertx(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
#endif  // CONFIG_SUPERTX
void av1_encode_sby_pass1(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
void av1_xform_quant(const AV1_COMMON *cm, MACROBLOCK *x, int plane, int block,
                     int blk_row, int blk_col, BLOCK_SIZE plane_bsize,
                     TX_SIZE tx_size, AV1_XFORM_QUANT xform_quant_idx);
#if CONFIG_NEW_QUANT
void av1_xform_quant_nuq(const AV1_COMMON *cm, MACROBLOCK *x, int plane,
                         int block, int blk_row, int blk_col,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size, int ctx);
void av1_xform_quant_dc_nuq(MACROBLOCK *x, int plane, int block, int blk_row,
                            int blk_col, BLOCK_SIZE plane_bsize,
                            TX_SIZE tx_size, int ctx);
void av1_xform_quant_fp_nuq(const AV1_COMMON *cm, MACROBLOCK *x, int plane,
                            int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size, int ctx);
void av1_xform_quant_dc_fp_nuq(MACROBLOCK *x, int plane, int block, int blk_row,
                               int blk_col, BLOCK_SIZE plane_bsize,
                               TX_SIZE tx_size, int ctx);
#endif

int av1_optimize_b(const AV1_COMMON *cm, MACROBLOCK *mb, int plane, int block,
                   TX_SIZE tx_size, int ctx);

void av1_subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane);

void av1_encode_block_intra(int plane, int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg);

void av1_encode_intra_block_plane(AV1_COMMON *cm, MACROBLOCK *x,
                                  BLOCK_SIZE bsize, int plane,
                                  int enable_optimize_b);
=======
#include "av1/common/onyxc_int.h"
#include "av1/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

struct encode_b_args {
  AV1_COMMON *cm;
  MACROBLOCK *x;
  struct optimize_ctx *ctx;
  int8_t *skip;
};
void av1_encode_sb(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
void av1_encode_sby_pass1(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
void av1_xform_quant_fp(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                        int block, int blk_row, int blk_col,
                        BLOCK_SIZE plane_bsize, TX_SIZE tx_size);
void av1_xform_quant(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                     int block, int blk_row, int blk_col,
                     BLOCK_SIZE plane_bsize, TX_SIZE tx_size);

void av1_subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane);

void av1_encode_block_intra(int plane, int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg);

void av1_encode_intra_block_plane(AV1_COMMON *cm, MACROBLOCK *x,
                                  BLOCK_SIZE bsize, int plane);
>>>>>>> BRANCH (5bf37c Use --enable-daala_ec by default.)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_ENCODEMB_H_
