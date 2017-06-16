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

#include "av1/common/cfl.h"
#include "av1/common/common_data.h"
#include "av1/common/onyxc_int.h"

#include "aom/internal/aom_codec_internal.h"

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm) {
  if (!((cm->subsampling_x == 0 && cm->subsampling_y == 0) ||
        (cm->subsampling_x == 1 && cm->subsampling_y == 1))) {
    aom_internal_error(&cm->error, AOM_CODEC_UNSUP_BITSTREAM,
                       "Only 4:4:4 and 4:2:0 are currently supported by CfL");
  }
  memset(&cfl->y_pix, 0, sizeof(uint8_t) * MAX_SB_SQUARE);
  cfl->subsampling_x = cm->subsampling_x;
  cfl->subsampling_y = cm->subsampling_y;
  cfl->are_parameters_computed = 0;
}

// Load from the CfL pixel buffer into output
static void cfl_load(CFL_CTX *cfl, int row, int col, int width, int height) {
  const int sub_x = cfl->subsampling_x;
  const int sub_y = cfl->subsampling_y;
  const int off_log2 = tx_size_wide_log2[0];

  // TODO(ltrudeau) convert to uint16 to add HBD support
  const uint8_t *y_pix;
  // TODO(ltrudeau) convert to uint16 to add HBD support
  uint8_t *output = cfl->y_down_pix;

  int pred_row_offset = 0;
  int output_row_offset = 0;

  // TODO(ltrudeau) should be faster to downsample when we store the values
  // TODO(ltrudeau) add support for 4:2:2
  if (sub_y == 0 && sub_x == 0) {
    y_pix = &cfl->y_pix[(row * MAX_SB_SIZE + col) << off_log2];
    for (int j = 0; j < height; j++) {
      for (int i = 0; i < width; i++) {
        // In 4:4:4, pixels match 1 to 1
        output[output_row_offset + i] = y_pix[pred_row_offset + i];
      }
      pred_row_offset += MAX_SB_SIZE;
      output_row_offset += MAX_SB_SIZE;
    }
  } else if (sub_y == 1 && sub_x == 1) {
    y_pix = &cfl->y_pix[(row * MAX_SB_SIZE + col) << (off_log2 + sub_y)];
    for (int j = 0; j < height; j++) {
      for (int i = 0; i < width; i++) {
        int top_left = (pred_row_offset + i) << sub_y;
        int bot_left = top_left + MAX_SB_SIZE;
        // In 4:2:0, average pixels in 2x2 grid
        output[output_row_offset + i] = OD_SHR_ROUND(
            y_pix[top_left] + y_pix[top_left + 1]        // Top row
                + y_pix[bot_left] + y_pix[bot_left + 1]  // Bottom row
            ,
            2);
      }
      pred_row_offset += MAX_SB_SIZE;
      output_row_offset += MAX_SB_SIZE;
    }
  } else {
    assert(0);  // Unsupported chroma subsampling
  }
  // Due to frame boundary issues, it is possible that the total area of
  // covered by Chroma exceeds that of Luma. When this happens, we write over
  // the broken data by repeating the last columns and/or rows.
  //
  // Note that in order to manage the case where both rows and columns
  // overrun,
  // we apply rows first. This way, when the rows overrun the bottom of the
  // frame, the columns will be copied over them.
  const int uv_width = (col << off_log2) + width;
  const int uv_height = (row << off_log2) + height;

  const int diff_width = uv_width - (cfl->y_width >> sub_x);
  const int diff_height = uv_height - (cfl->y_height >> sub_y);

  if (diff_width > 0) {
    int last_pixel;
    output_row_offset = width - diff_width;

    for (int j = 0; j < height; j++) {
      last_pixel = output_row_offset - 1;
      for (int i = 0; i < diff_width; i++) {
        output[output_row_offset + i] = output[last_pixel];
      }
      output_row_offset += MAX_SB_SIZE;
    }
  }

  if (diff_height > 0) {
    output_row_offset = (height - diff_height) * MAX_SB_SIZE;
    const int last_row_offset = output_row_offset - MAX_SB_SIZE;

    for (int j = 0; j < diff_height; j++) {
      for (int i = 0; i < width; i++) {
        output[output_row_offset + i] = output[last_row_offset + i];
      }
      output_row_offset += MAX_SB_SIZE;
    }
  }
}

// CfL computes its own block-level DC_PRED. This is required to compute both
// alpha_cb and alpha_cr before the prediction are computed.
static void cfl_dc_pred(MACROBLOCKD *xd, BLOCK_SIZE plane_bsize) {
  const struct macroblockd_plane *const pd_u = &xd->plane[AOM_PLANE_U];
  const struct macroblockd_plane *const pd_v = &xd->plane[AOM_PLANE_V];

  const uint8_t *const dst_u = pd_u->dst.buf;
  const uint8_t *const dst_v = pd_v->dst.buf;

  const int dst_u_stride = pd_u->dst.stride;
  const int dst_v_stride = pd_v->dst.stride;

  CFL_CTX *const cfl = xd->cfl;

  // Compute DC_PRED until block boundary. We can't assume the neighbor will use
  // the same transform size.
  const int width = max_block_wide(xd, plane_bsize, AOM_PLANE_U)
                    << tx_size_wide_log2[0];
  const int height = max_block_high(xd, plane_bsize, AOM_PLANE_U)
                     << tx_size_high_log2[0];
  // Number of pixel on the top and left borders.
  const double num_pel = width + height;

  int sum_u = 0;
  int sum_v = 0;

// Match behavior of build_intra_predictors (reconintra.c) at superblock
// boundaries:
//
// 127 127 127 .. 127 127 127 127 127 127
// 129  A   B  ..  Y   Z
// 129  C   D  ..  W   X
// 129  E   F  ..  U   V
// 129  G   H  ..  S   T   T   T   T   T
// ..

#if CONFIG_CHROMA_SUB8X8
  if (xd->chroma_up_available && xd->mb_to_right_edge >= 0) {
#else
  if (xd->up_available && xd->mb_to_right_edge >= 0) {
#endif
    // TODO(ltrudeau) replace this with DC_PRED assembly
    for (int i = 0; i < width; i++) {
      sum_u += dst_u[-dst_u_stride + i];
      sum_v += dst_v[-dst_v_stride + i];
    }
  } else {
    sum_u = width * 127;
    sum_v = width * 127;
  }

#if CONFIG_CHROMA_SUB8X8
  if (xd->chroma_left_available && xd->mb_to_bottom_edge >= 0) {
#else
  if (xd->left_available && xd->mb_to_bottom_edge >= 0) {
#endif
    for (int i = 0; i < height; i++) {
      sum_u += dst_u[i * dst_u_stride - 1];
      sum_v += dst_v[i * dst_v_stride - 1];
    }
  } else {
    sum_u += height * 129;
    sum_v += height * 129;
  }

  // TODO(ltrudeau) Because of max_block_wide and max_block_height, numpel will
  // not be a power of two. So these divisions will have to use a lookup table.
  cfl->dc_pred[CFL_PRED_U] = sum_u / num_pel;
  cfl->dc_pred[CFL_PRED_V] = sum_v / num_pel;
}

static void cfl_compute_average(CFL_CTX *cfl) {
  const int width = cfl->uv_width;
  const int height = cfl->uv_height;
  const double num_pel = width * height;
  // TODO(ltrudeau) Convert to uint16 for HBD support
  const uint8_t *y_pix = cfl->y_down_pix;
  // TODO(ltrudeau) Convert to uint16 for HBD support

  cfl_load(cfl, 0, 0, width, height);

  int sum = 0;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      sum += y_pix[i];
    }
    y_pix += MAX_SB_SIZE;
  }
  cfl->y_average = sum / num_pel;
}

static INLINE double cfl_idx_to_alpha(int alpha_idx, CFL_SIGN_TYPE alpha_sign,
                                      CFL_PRED_TYPE pred_type) {
  const int mag_idx = cfl_alpha_codes[alpha_idx][pred_type];
  const double abs_alpha = cfl_alpha_mags[mag_idx];
  if (alpha_sign == CFL_SIGN_POS) {
    return abs_alpha;
  } else {
    assert(abs_alpha != 0.0);
    assert(cfl_alpha_mags[mag_idx + 1] == -abs_alpha);
    return -abs_alpha;
  }
}

// Predict the current transform block using CfL.
void cfl_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       int row, int col, TX_SIZE tx_size, int plane) {
  CFL_CTX *const cfl = xd->cfl;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

  // CfL parameters must be computed before prediction can be done.
  assert(cfl->are_parameters_computed == 1);

  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  // TODO(ltrudeau) Convert to uint16 to support HBD
  const uint8_t *y_pix = cfl->y_down_pix;

  const double dc_pred = cfl->dc_pred[plane - 1];
  const double alpha = cfl_idx_to_alpha(
      mbmi->cfl_alpha_idx, mbmi->cfl_alpha_signs[plane - 1], plane - 1);

  const double avg = cfl->y_average;

  cfl_load(cfl, row, col, width, height);
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      dst[i] = (uint8_t)(alpha * (y_pix[i] - avg) + dc_pred + 0.5);
    }
    dst += dst_stride;
    y_pix += MAX_SB_SIZE;
  }
}

void cfl_store(CFL_CTX *cfl, const uint8_t *input, int input_stride, int row,
               int col, TX_SIZE tx_size, BLOCK_SIZE bsize) {
  const int tx_width = tx_size_wide[tx_size];
  const int tx_height = tx_size_high[tx_size];
  const int tx_off_log2 = tx_size_wide_log2[0];

#if CONFIG_CHROMA_SUB8X8
  if (bsize < BLOCK_8X8) {
    // For chroma_sub8x8, the CfL prediction for prediction blocks smaller than
    // 8X8 uses non chroma reference reconstructed luma pixels. To do so, we
    // combine the 4X4 non chroma reference into the CfL pixel buffers based on
    // their row and column index.

    // The following code is adapted from the is_chroma_reference() function.

    // Not the block width, but the prediction block partitioning width
    const int bw = mi_size_wide[bsize];
    // Not the block height, but the Prediction block partitioning height
    const int bh = mi_size_high[bsize];

    if ((cfl->mi_row &
         0x01)        // Increment the row index for odd indexed 4X4 blocks
        && (bw == 4)  // But not for 8X4 blocks
        && cfl->subsampling_y) {  // And only when chroma is subsampled
      assert(row == 0);
      row++;
    }

    if ((cfl->mi_col &
         0x01)        // Increment the col index for odd indexed 4X4 blocks
        && (bh == 4)  // But not for 4X8 blocks
        && cfl->subsampling_x) {  // And only when chroma is subsampled
      assert(col == 0);
      col++;
    }
  }
#endif

  // Invalidate current parameters
  cfl->are_parameters_computed = 0;

  // Store the surface of the pixel buffer that was written to, this way we
  // can manage chroma overrun (e.g. when the chroma surfaces goes beyond the
  // frame boundary)
  if (col == 0 && row == 0) {
    cfl->y_width = tx_width;
    cfl->y_height = tx_height;
  } else {
    cfl->y_width = OD_MAXI((col << tx_off_log2) + tx_width, cfl->y_width);
    cfl->y_height = OD_MAXI((row << tx_off_log2) + tx_height, cfl->y_height);
  }

  // Check that we will remain inside the pixel buffer.
  // TODO(ltrudeau) This is broken, fix it.
  assert(MAX_SB_SIZE * (row + tx_height - 1) + col + tx_width - 1 <
         MAX_SB_SQUARE);

  // Store the input into the CfL pixel buffer
  uint8_t *y_pix = &cfl->y_pix[(row * MAX_SB_SIZE + col) << tx_off_log2];

  // TODO(ltrudeau) Speedup possible by moving the downsampling to cfl_store
  for (int j = 0; j < tx_height; j++) {
    for (int i = 0; i < tx_width; i++) {
      y_pix[i] = input[i];
    }
    y_pix += MAX_SB_SIZE;
    input += input_stride;
  }
}

void cfl_compute_parameters(MACROBLOCKD *const xd, TX_SIZE tx_size) {
  CFL_CTX *const cfl = xd->cfl;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;

  // Do not call cfl_compute_parameters multiple time on the same values.
  assert(cfl->are_parameters_computed == 0);

#if CONFIG_CHROMA_SUB8X8
  const BLOCK_SIZE plane_bsize = AOMMAX(
      BLOCK_4X4, get_plane_block_size(mbmi->sb_type, &xd->plane[AOM_PLANE_U]));
#else
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mbmi->sb_type, &xd->plane[AOM_PLANE_U]);
#endif
  // AOM_PLANE_U is used, but both planes will have the same sizes.
  cfl->uv_width = max_intra_block_width(xd, plane_bsize, AOM_PLANE_U, tx_size);
  cfl->uv_height =
      max_intra_block_height(xd, plane_bsize, AOM_PLANE_U, tx_size);

#if CONFIG_DEBUG
  if (mbmi->sb_type >= BLOCK_8X8) {
    assert(cfl->y_width <= cfl->uv_width << cfl->subsampling_x);
    assert(cfl->y_height <= cfl->uv_height << cfl->subsampling_y);
  }
#endif

  // Compute block-level DC_PRED for both chromatic planes.
  // DC_PRED replaces beta in the linear model.
  cfl_dc_pred(xd, plane_bsize);
  // Compute block-level average on reconstructed luma input.
  cfl_compute_average(cfl);
  cfl->are_parameters_computed = 1;
}
