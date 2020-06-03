/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// This tool is a gadget for offline probability training.
// A binary executable aom_entropy_optimizer will be generated in tools/. It
// parses a binary file consisting of counts written in the format of
// FRAME_COUNTS in entropymode.h, and computes optimized probability tables
// and CDF tables, which will be written to a new c file optimized_probs.c
// according to format in the codebase.
//
// Command line: ./aom_entropy_optimizer [directory of the count file]
//
// The input file can either be generated by encoding a single clip by
// turning on entropy_stats experiment, or be collected at a larger scale at
// which a python script which will be provided soon can be used to aggregate
// multiple stats output.

#include <assert.h>
#include <stdio.h>

#include "config/aom_config.h"

#include "av1/encoder/encoder.h"

#define SPACES_PER_TAB 2
#define CDF_MAX_SIZE 16

typedef unsigned int aom_count_type;
// A log file recording parsed counts
static FILE *logfile;  // TODO(yuec): make it a command line option

static void counts_to_cdf(const aom_count_type *counts, aom_cdf_prob *cdf,
                          int modes) {
  int64_t csum[CDF_MAX_SIZE];
  assert(modes <= CDF_MAX_SIZE);

  csum[0] = counts[0] + 1;
  for (int i = 1; i < modes; ++i) csum[i] = counts[i] + 1 + csum[i - 1];

  for (int i = 0; i < modes; ++i) fprintf(logfile, "%d ", counts[i]);
  fprintf(logfile, "\n");

  int64_t sum = csum[modes - 1];
  const int64_t round_shift = sum >> 1;
  for (int i = 0; i < modes; ++i) {
    cdf[i] = (csum[i] * CDF_PROB_TOP + round_shift) / sum;
    cdf[i] = AOMMIN(cdf[i], CDF_PROB_TOP - (modes - 1 + i) * 4);
    cdf[i] = (i == 0) ? AOMMAX(cdf[i], 4) : AOMMAX(cdf[i], cdf[i - 1] + 4);
  }
}

static int parse_counts_for_cdf_opt(aom_count_type **ct_ptr,
                                    FILE *const probsfile, int tabs,
                                    int dim_of_cts, int *cts_each_dim) {
  if (dim_of_cts < 1) {
    fprintf(stderr, "The dimension of a counts vector should be at least 1!\n");
    return 1;
  }
  const int total_modes = cts_each_dim[0];
  if (dim_of_cts == 1) {
    assert(total_modes <= CDF_MAX_SIZE);
    aom_cdf_prob cdfs[CDF_MAX_SIZE];
    aom_count_type *counts1d = *ct_ptr;

    counts_to_cdf(counts1d, cdfs, total_modes);
    (*ct_ptr) += total_modes;

    if (tabs > 0) fprintf(probsfile, "%*c", tabs * SPACES_PER_TAB, ' ');
    fprintf(probsfile, "AOM_CDF%d(", total_modes);
    for (int k = 0; k < total_modes - 1; ++k) {
      fprintf(probsfile, "%d", cdfs[k]);
      if (k < total_modes - 2) fprintf(probsfile, ", ");
    }
    fprintf(probsfile, ")");
  } else {
    for (int k = 0; k < total_modes; ++k) {
      int tabs_next_level;

      if (dim_of_cts == 2)
        fprintf(probsfile, "%*c{ ", tabs * SPACES_PER_TAB, ' ');
      else
        fprintf(probsfile, "%*c{\n", tabs * SPACES_PER_TAB, ' ');
      tabs_next_level = dim_of_cts == 2 ? 0 : tabs + 1;

      if (parse_counts_for_cdf_opt(ct_ptr, probsfile, tabs_next_level,
                                   dim_of_cts - 1, cts_each_dim + 1)) {
        return 1;
      }

      if (dim_of_cts == 2) {
        if (k == total_modes - 1)
          fprintf(probsfile, " }\n");
        else
          fprintf(probsfile, " },\n");
      } else {
        if (k == total_modes - 1)
          fprintf(probsfile, "%*c}\n", tabs * SPACES_PER_TAB, ' ');
        else
          fprintf(probsfile, "%*c},\n", tabs * SPACES_PER_TAB, ' ');
      }
    }
  }
  return 0;
}

static void optimize_cdf_table(aom_count_type *counts, FILE *const probsfile,
                               int dim_of_cts, int *cts_each_dim,
                               char *prefix) {
  aom_count_type *ct_ptr = counts;

  fprintf(probsfile, "%s = {\n", prefix);
  fprintf(logfile, "%s\n", prefix);
  if (parse_counts_for_cdf_opt(&ct_ptr, probsfile, 1, dim_of_cts,
                               cts_each_dim)) {
    fprintf(probsfile, "Optimizer failed!\n");
  }
  fprintf(probsfile, "};\n\n");
  fprintf(logfile, "============================\n");
}

static void optimize_uv_mode(aom_count_type *counts, FILE *const probsfile,
                             int dim_of_cts, int *cts_each_dim, char *prefix) {
  aom_count_type *ct_ptr = counts;

  fprintf(probsfile, "%s = {\n", prefix);
  fprintf(probsfile, "%*c{\n", SPACES_PER_TAB, ' ');
  fprintf(logfile, "%s\n", prefix);
  cts_each_dim[2] = UV_INTRA_MODES - 1;
  for (int k = 0; k < cts_each_dim[1]; ++k) {
    fprintf(probsfile, "%*c{ ", 2 * SPACES_PER_TAB, ' ');
    parse_counts_for_cdf_opt(&ct_ptr, probsfile, 0, dim_of_cts - 2,
                             cts_each_dim + 2);
    if (k + 1 == cts_each_dim[1]) {
      fprintf(probsfile, " }\n");
    } else {
      fprintf(probsfile, " },\n");
    }
    ++ct_ptr;
  }
  fprintf(probsfile, "%*c},\n", SPACES_PER_TAB, ' ');
  fprintf(probsfile, "%*c{\n", SPACES_PER_TAB, ' ');
  cts_each_dim[2] = UV_INTRA_MODES;
  parse_counts_for_cdf_opt(&ct_ptr, probsfile, 2, dim_of_cts - 1,
                           cts_each_dim + 1);
  fprintf(probsfile, "%*c}\n", SPACES_PER_TAB, ' ');
  fprintf(probsfile, "};\n\n");
  fprintf(logfile, "============================\n");
}

static void optimize_cdf_table_var_modes_2d(aom_count_type *counts,
                                            FILE *const probsfile,
                                            int dim_of_cts, int *cts_each_dim,
                                            int *modes_each_ctx, char *prefix) {
  aom_count_type *ct_ptr = counts;

  assert(dim_of_cts == 2);
  (void)dim_of_cts;

  fprintf(probsfile, "%s = {\n", prefix);
  fprintf(logfile, "%s\n", prefix);

  for (int d0_idx = 0; d0_idx < cts_each_dim[0]; ++d0_idx) {
    int num_of_modes = modes_each_ctx[d0_idx];

    if (num_of_modes > 0) {
      fprintf(probsfile, "%*c{ ", SPACES_PER_TAB, ' ');
      parse_counts_for_cdf_opt(&ct_ptr, probsfile, 0, 1, &num_of_modes);
      ct_ptr += cts_each_dim[1] - num_of_modes;
      fprintf(probsfile, " },\n");
    } else {
      fprintf(probsfile, "%*c{ 0 },\n", SPACES_PER_TAB, ' ');
      fprintf(logfile, "dummy cdf, no need to optimize\n");
      ct_ptr += cts_each_dim[1];
    }
  }
  fprintf(probsfile, "};\n\n");
  fprintf(logfile, "============================\n");
}

static void optimize_cdf_table_var_modes_3d(aom_count_type *counts,
                                            FILE *const probsfile,
                                            int dim_of_cts, int *cts_each_dim,
                                            int *modes_each_ctx, char *prefix) {
  aom_count_type *ct_ptr = counts;

  assert(dim_of_cts == 3);
  (void)dim_of_cts;

  fprintf(probsfile, "%s = {\n", prefix);
  fprintf(logfile, "%s\n", prefix);

  for (int d0_idx = 0; d0_idx < cts_each_dim[0]; ++d0_idx) {
    fprintf(probsfile, "%*c{\n", SPACES_PER_TAB, ' ');
    for (int d1_idx = 0; d1_idx < cts_each_dim[1]; ++d1_idx) {
      int num_of_modes = modes_each_ctx[d0_idx];

      if (num_of_modes > 0) {
        fprintf(probsfile, "%*c{ ", 2 * SPACES_PER_TAB, ' ');
        parse_counts_for_cdf_opt(&ct_ptr, probsfile, 0, 1, &num_of_modes);
        ct_ptr += cts_each_dim[2] - num_of_modes;
        fprintf(probsfile, " },\n");
      } else {
        fprintf(probsfile, "%*c{ 0 },\n", 2 * SPACES_PER_TAB, ' ');
        fprintf(logfile, "dummy cdf, no need to optimize\n");
        ct_ptr += cts_each_dim[2];
      }
    }
    fprintf(probsfile, "%*c},\n", SPACES_PER_TAB, ' ');
  }
  fprintf(probsfile, "};\n\n");
  fprintf(logfile, "============================\n");
}

static void optimize_cdf_table_var_modes_4d(aom_count_type *counts,
                                            FILE *const probsfile,
                                            int dim_of_cts, int *cts_each_dim,
                                            int *modes_each_ctx, char *prefix) {
  aom_count_type *ct_ptr = counts;

  assert(dim_of_cts == 4);
  (void)dim_of_cts;

  fprintf(probsfile, "%s = {\n", prefix);
  fprintf(logfile, "%s\n", prefix);

  for (int d0_idx = 0; d0_idx < cts_each_dim[0]; ++d0_idx) {
    fprintf(probsfile, "%*c{\n", SPACES_PER_TAB, ' ');
    for (int d1_idx = 0; d1_idx < cts_each_dim[1]; ++d1_idx) {
      fprintf(probsfile, "%*c{\n", 2 * SPACES_PER_TAB, ' ');
      for (int d2_idx = 0; d2_idx < cts_each_dim[2]; ++d2_idx) {
        int num_of_modes = modes_each_ctx[d0_idx];

        if (num_of_modes > 0) {
          fprintf(probsfile, "%*c{ ", 3 * SPACES_PER_TAB, ' ');
          parse_counts_for_cdf_opt(&ct_ptr, probsfile, 0, 1, &num_of_modes);
          ct_ptr += cts_each_dim[3] - num_of_modes;
          fprintf(probsfile, " },\n");
        } else {
          fprintf(probsfile, "%*c{ 0 },\n", 3 * SPACES_PER_TAB, ' ');
          fprintf(logfile, "dummy cdf, no need to optimize\n");
          ct_ptr += cts_each_dim[3];
        }
      }
      fprintf(probsfile, "%*c},\n", 2 * SPACES_PER_TAB, ' ');
    }
    fprintf(probsfile, "%*c},\n", SPACES_PER_TAB, ' ');
  }
  fprintf(probsfile, "};\n\n");
  fprintf(logfile, "============================\n");
}

int main(int argc, const char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Please specify the input stats file!\n");
    exit(EXIT_FAILURE);
  }

  FILE *const statsfile = fopen(argv[1], "rb");
  if (statsfile == NULL) {
    fprintf(stderr, "Failed to open input file!\n");
    exit(EXIT_FAILURE);
  }

  FRAME_COUNTS fc;
  const size_t bytes = fread(&fc, sizeof(FRAME_COUNTS), 1, statsfile);
  if (!bytes) return 1;

  FILE *const probsfile = fopen("optimized_probs.c", "w");
  if (probsfile == NULL) {
    fprintf(stderr,
            "Failed to create output file for optimized entropy tables!\n");
    exit(EXIT_FAILURE);
  }

  logfile = fopen("aom_entropy_optimizer_parsed_counts.log", "w");
  if (logfile == NULL) {
    fprintf(stderr, "Failed to create log file for parsed counts!\n");
    exit(EXIT_FAILURE);
  }

  int cts_each_dim[10];

  /* Intra mode (keyframe luma) */
  cts_each_dim[0] = KF_MODE_CONTEXTS;
  cts_each_dim[1] = KF_MODE_CONTEXTS;
  cts_each_dim[2] = INTRA_MODES;
  optimize_cdf_table(&fc.kf_y_mode[0][0][0], probsfile, 3, cts_each_dim,
                     "const aom_cdf_prob\n"
                     "default_kf_y_mode_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS]"
                     "[CDF_SIZE(INTRA_MODES)]");

  cts_each_dim[0] = DIRECTIONAL_MODES;
  cts_each_dim[1] = 2 * MAX_ANGLE_DELTA + 1;
  optimize_cdf_table(&fc.angle_delta[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob default_angle_delta_cdf"
                     "[DIRECTIONAL_MODES][CDF_SIZE(2 * MAX_ANGLE_DELTA + 1)]");

  /* Intra mode (non-keyframe luma) */
  cts_each_dim[0] = BLOCK_SIZE_GROUPS;
  cts_each_dim[1] = INTRA_MODES;
  optimize_cdf_table(
      &fc.y_mode[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTRA_MODES)]");

  /* Intra mode (chroma) */
  cts_each_dim[0] = CFL_ALLOWED_TYPES;
  cts_each_dim[1] = INTRA_MODES;
  cts_each_dim[2] = UV_INTRA_MODES;
  optimize_uv_mode(&fc.uv_mode[0][0][0], probsfile, 3, cts_each_dim,
                   "static const aom_cdf_prob\n"
                   "default_uv_mode_cdf[CFL_ALLOWED_TYPES][INTRA_MODES]"
                   "[CDF_SIZE(UV_INTRA_MODES)]");

  /* block partition */
  cts_each_dim[0] = PARTITION_CONTEXTS;
  cts_each_dim[1] = EXT_PARTITION_TYPES;
#if CONFIG_EXT_RECUR_PARTITIONS
  int part_types_each_ctx[PARTITION_CONTEXTS] = {
    3, 3, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 3, 3, 3
  };
#else
  int part_types_each_ctx[PARTITION_CONTEXTS] = { 4,  4,  4,  4,  10, 10, 10,
                                                  10, 10, 10, 10, 10, 10, 10,
                                                  10, 10, 8,  8,  8,  8 };
#endif  // CONFIG_EXT_RECUR_PARTITIONS
  optimize_cdf_table_var_modes_2d(
      &fc.partition[0][0], probsfile, 2, cts_each_dim, part_types_each_ctx,
      "static const aom_cdf_prob default_partition_cdf[PARTITION_CONTEXTS]"
      "[CDF_SIZE(EXT_PARTITION_TYPES)]");

#if CONFIG_EXT_RECUR_PARTITIONS
  cts_each_dim[0] = PARTITION_CONTEXTS_REC;
  cts_each_dim[1] = PARTITION_TYPES_REC;
  int part_types_each_ctx_rec[PARTITION_CONTEXTS_REC] = { 2, 2, 2, 2, 4, 4, 4,
                                                          4, 4, 4, 4, 4, 4, 4,
                                                          4, 4, 3, 3, 3, 3 };
  optimize_cdf_table_var_modes_2d(
      &fc.partition_rec[0][0], probsfile, 2, cts_each_dim,
      part_types_each_ctx_rec,
      "static const aom_cdf_prob "
      "default_partition_rec_cdf[PARTITION_CONTEXTS_REC]"
      "[CDF_SIZE(PARTITION_TYPES_REC)]");
#endif  // CONFIG_EXT_RECUR_PARTITIONS

  /* mdt_type */
#if CONFIG_MODE_DEP_INTER_TX
  cts_each_dim[0] = EXT_TX_SIZES;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.use_mdtx_inter[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob default_use_mdtx_inter[EXT_TX_SIZES]"
      "[CDF_SIZE(2)]");

  cts_each_dim[0] = EXT_TX_SIZES;
  cts_each_dim[1] = MDTX_TYPES_INTER;
  optimize_cdf_table(&fc.mdtx_type_inter[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_mdtx_type_inter[EXT_TX_SIZES]"
                     "[CDF_SIZE(MDTX_TYPES_INTER)]");
#endif
#if CONFIG_MODE_DEP_INTRA_TX
  cts_each_dim[0] = EXT_TX_SIZES;
  cts_each_dim[1] = INTRA_MODES;
  cts_each_dim[2] = 2;
  optimize_cdf_table(
      &fc.use_mdtx_intra[0][0][0], probsfile, 3, cts_each_dim,
      "static const aom_cdf_prob default_use_mdtx_intra[EXT_TX_SIZES]"
      "[INTRA_MODES][CDF_SIZE(2)]");

  cts_each_dim[0] = EXT_TX_SIZES;
  cts_each_dim[1] = INTRA_MODES;
  cts_each_dim[2] = MDTX_TYPES_INTRA;
  optimize_cdf_table(&fc.mdtx_type_intra[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_mdtx_type_intra[EXT_TX_SIZES][INTRA_MODES]"
                     "[CDF_SIZE(MDTX_TYPES_INTRA)]");
#endif

  /* tx type */
  cts_each_dim[0] = EXT_TX_SETS_INTRA;
  cts_each_dim[1] = EXT_TX_SIZES;
  cts_each_dim[2] = INTRA_MODES;
#if CONFIG_MODE_DEP_INTRA_TX || CONFIG_MODE_DEP_INTER_TX
  cts_each_dim[3] = TX_TYPES_NOMDTX;
#else
  cts_each_dim[3] = TX_TYPES;
#endif
  int intra_ext_tx_types_each_ctx[EXT_TX_SETS_INTRA] = { 0, 7, 5 };
  optimize_cdf_table_var_modes_4d(
      &fc.intra_ext_tx[0][0][0][0], probsfile, 4, cts_each_dim,
      intra_ext_tx_types_each_ctx,
      "static const aom_cdf_prob default_intra_ext_tx_cdf[EXT_TX_SETS_INTRA]"
      "[EXT_TX_SIZES][INTRA_MODES][CDF_SIZE(TX_TYPES)]");

  cts_each_dim[0] = EXT_TX_SETS_INTER;
  cts_each_dim[1] = EXT_TX_SIZES;
#if CONFIG_MODE_DEP_INTRA_TX || CONFIG_MODE_DEP_INTER_TX
  cts_each_dim[2] = TX_TYPES_NOMDTX;
#else
  cts_each_dim[2] = TX_TYPES;
#endif
  int inter_ext_tx_types_each_ctx[EXT_TX_SETS_INTER] = { 0, 16, 12, 2 };
  optimize_cdf_table_var_modes_3d(
      &fc.inter_ext_tx[0][0][0], probsfile, 3, cts_each_dim,
      inter_ext_tx_types_each_ctx,
      "static const aom_cdf_prob default_inter_ext_tx_cdf[EXT_TX_SETS_INTER]"
      "[EXT_TX_SIZES][CDF_SIZE(TX_TYPES)]");

  /* Chroma from Luma */
  cts_each_dim[0] = CFL_JOINT_SIGNS;
  optimize_cdf_table(&fc.cfl_sign[0], probsfile, 1, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_cfl_sign_cdf[CDF_SIZE(CFL_JOINT_SIGNS)]");
  cts_each_dim[0] = CFL_ALPHA_CONTEXTS;
  cts_each_dim[1] = CFL_ALPHABET_SIZE;
  optimize_cdf_table(&fc.cfl_alpha[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_cfl_alpha_cdf[CFL_ALPHA_CONTEXTS]"
                     "[CDF_SIZE(CFL_ALPHABET_SIZE)]");

  /* Interpolation filter */
  cts_each_dim[0] = SWITCHABLE_FILTER_CONTEXTS;
  cts_each_dim[1] = SWITCHABLE_FILTERS;
  optimize_cdf_table(&fc.switchable_interp[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS]"
                     "[CDF_SIZE(SWITCHABLE_FILTERS)]");

  /* Motion vector referencing */
  cts_each_dim[0] = NEWMV_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.newmv_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = GLOBALMV_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.zeromv_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_zeromv_cdf[GLOBALMV_MODE_CONTEXTS][CDF_SIZE(2)]");

#if CONFIG_NEW_INTER_MODES
  cts_each_dim[0] = DRL_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.drl0_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_drl0_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)]");
  optimize_cdf_table(&fc.drl1_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_drl1_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)]");
  optimize_cdf_table(&fc.drl2_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_drl2_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)]");
#else
  cts_each_dim[0] = REFMV_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.refmv_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = DRL_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.drl_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)]");
#endif  // CONFIG_NEW_INTER_MODES

  /* ext_inter experiment */
  /* New compound mode */
  cts_each_dim[0] = INTER_MODE_CONTEXTS;
  cts_each_dim[1] = INTER_COMPOUND_MODES;
  optimize_cdf_table(&fc.inter_compound_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_inter_compound_mode_cdf[INTER_MODE_CONTEXTS][CDF_"
                     "SIZE(INTER_COMPOUND_MODES)]");

  /* Interintra */
  cts_each_dim[0] = BLOCK_SIZE_GROUPS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.interintra[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_interintra_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(2)]");

  cts_each_dim[0] = BLOCK_SIZE_GROUPS;
  cts_each_dim[1] = INTERINTRA_MODES;
  optimize_cdf_table(&fc.interintra_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_interintra_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE("
                     "INTERINTRA_MODES)]");

  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.wedge_interintra[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_wedge_interintra_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)]");

  /* Compound type */
  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = COMPOUND_TYPES - 1;
  optimize_cdf_table(&fc.compound_type[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob default_compound_type_cdf"
                     "[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES - 1)]");

  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 16;
  optimize_cdf_table(&fc.wedge_idx[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_wedge_idx_cdf[BLOCK_SIZES_ALL][CDF_SIZE(16)]");

  /* motion_var and warped_motion experiments */
  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = MOTION_MODES;
  optimize_cdf_table(
      &fc.motion_mode[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)]");
  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.obmc[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)]");

  /* Intra/inter flag */
  cts_each_dim[0] = INTRA_INTER_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.intra_inter[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_intra_inter_cdf[INTRA_INTER_CONTEXTS][CDF_SIZE(2)]");

  /* Single/comp ref flag */
  cts_each_dim[0] = COMP_INTER_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.comp_inter[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(2)]");

  /* ext_comp_refs experiment */
  cts_each_dim[0] = COMP_REF_TYPE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.comp_ref_type[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = UNI_COMP_REF_CONTEXTS;
  cts_each_dim[1] = UNIDIR_COMP_REFS - 1;
  cts_each_dim[2] = 2;
  optimize_cdf_table(&fc.uni_comp_ref[0][0][0], probsfile, 3, cts_each_dim,
                     "static const aom_cdf_prob\n"
                     "default_uni_comp_ref_cdf[UNI_COMP_REF_CONTEXTS][UNIDIR_"
                     "COMP_REFS - 1][CDF_SIZE(2)]");

  /* Reference frame (single ref) */
  cts_each_dim[0] = REF_CONTEXTS;
  cts_each_dim[1] = SINGLE_REFS - 1;
  cts_each_dim[2] = 2;
  optimize_cdf_table(
      &fc.single_ref[0][0][0], probsfile, 3, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_single_ref_cdf[REF_CONTEXTS][SINGLE_REFS - 1][CDF_SIZE(2)]");

  /* ext_refs experiment */
  cts_each_dim[0] = REF_CONTEXTS;
  cts_each_dim[1] = FWD_REFS - 1;
  cts_each_dim[2] = 2;
  optimize_cdf_table(
      &fc.comp_ref[0][0][0], probsfile, 3, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_comp_ref_cdf[REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)]");

  cts_each_dim[0] = REF_CONTEXTS;
  cts_each_dim[1] = BWD_REFS - 1;
  cts_each_dim[2] = 2;
  optimize_cdf_table(
      &fc.comp_bwdref[0][0][0], probsfile, 3, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_comp_bwdref_cdf[REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)]");

  /* palette */
  cts_each_dim[0] = PALATTE_BSIZE_CTXS;
  cts_each_dim[1] = PALETTE_SIZES;
  optimize_cdf_table(&fc.palette_y_size[0][0], probsfile, 2, cts_each_dim,
                     "const aom_cdf_prob default_palette_y_size_cdf"
                     "[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)]");

  cts_each_dim[0] = PALATTE_BSIZE_CTXS;
  cts_each_dim[1] = PALETTE_SIZES;
  optimize_cdf_table(&fc.palette_uv_size[0][0], probsfile, 2, cts_each_dim,
                     "const aom_cdf_prob default_palette_uv_size_cdf"
                     "[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)]");

  cts_each_dim[0] = PALATTE_BSIZE_CTXS;
  cts_each_dim[1] = PALETTE_Y_MODE_CONTEXTS;
  cts_each_dim[2] = 2;
  optimize_cdf_table(&fc.palette_y_mode[0][0][0], probsfile, 3, cts_each_dim,
                     "const aom_cdf_prob default_palette_y_mode_cdf"
                     "[PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS]"
                     "[CDF_SIZE(2)]");

  cts_each_dim[0] = PALETTE_UV_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.palette_uv_mode[0][0], probsfile, 2, cts_each_dim,
                     "const aom_cdf_prob default_palette_uv_mode_cdf"
                     "[PALETTE_UV_MODE_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = PALETTE_SIZES;
  cts_each_dim[1] = PALETTE_COLOR_INDEX_CONTEXTS;
  cts_each_dim[2] = PALETTE_COLORS;
  int palette_color_indexes_each_ctx[PALETTE_SIZES] = { 2, 3, 4, 5, 6, 7, 8 };
  optimize_cdf_table_var_modes_3d(
      &fc.palette_y_color_index[0][0][0], probsfile, 3, cts_each_dim,
      palette_color_indexes_each_ctx,
      "const aom_cdf_prob default_palette_y_color_index_cdf[PALETTE_SIZES]"
      "[PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)]");

  cts_each_dim[0] = PALETTE_SIZES;
  cts_each_dim[1] = PALETTE_COLOR_INDEX_CONTEXTS;
  cts_each_dim[2] = PALETTE_COLORS;
  optimize_cdf_table_var_modes_3d(
      &fc.palette_uv_color_index[0][0][0], probsfile, 3, cts_each_dim,
      palette_color_indexes_each_ctx,
      "const aom_cdf_prob default_palette_uv_color_index_cdf[PALETTE_SIZES]"
      "[PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)]");

  /* Transform size */
  cts_each_dim[0] = TXFM_PARTITION_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.txfm_partition[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob\n"
      "default_txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)]");

  /* Skip flag */
  cts_each_dim[0] = SKIP_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.skip[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_skip_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)]");

  /* Skip mode flag */
  cts_each_dim[0] = SKIP_MODE_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.skip_mode[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_skip_mode_cdfs[SKIP_MODE_CONTEXTS][CDF_SIZE(2)]");

  /* joint compound flag */
  cts_each_dim[0] = COMP_INDEX_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.compound_index[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob default_compound_idx_cdfs"
                     "[COMP_INDEX_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = COMP_GROUP_IDX_CONTEXTS;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.comp_group_idx[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob default_comp_group_idx_cdfs"
                     "[COMP_GROUP_IDX_CONTEXTS][CDF_SIZE(2)]");

  /* intrabc */
  cts_each_dim[0] = 2;
  optimize_cdf_table(
      &fc.intrabc[0], probsfile, 1, cts_each_dim,
      "static const aom_cdf_prob default_intrabc_cdf[CDF_SIZE(2)]");

  /* filter_intra experiment */
  cts_each_dim[0] = FILTER_INTRA_MODES;
  optimize_cdf_table(
      &fc.filter_intra_mode[0], probsfile, 1, cts_each_dim,
      "static const aom_cdf_prob "
      "default_filter_intra_mode_cdf[CDF_SIZE(FILTER_INTRA_MODES)]");

  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 2;
  optimize_cdf_table(&fc.filter_intra[0][0], probsfile, 2, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_filter_intra_cdfs[BLOCK_SIZES_ALL][CDF_SIZE(2)]");

#if CONFIG_ADAPT_FILTER_INTRA
  /* adapt_filter_intra experiment */
  cts_each_dim[0] = USED_ADAPT_FILTER_INTRA_MODES;
  optimize_cdf_table(&fc.adapt_filter_intra_mode[0], probsfile, 1, cts_each_dim,
                     "static const aom_cdf_prob "
                     "default_adapt_filter_intra_mode_cdf[CDF_SIZE(USED_ADAPT_"
                     "FILTER_INTRA_MODES)]");

  cts_each_dim[0] = BLOCK_SIZES_ALL;
  cts_each_dim[1] = 2;
  optimize_cdf_table(
      &fc.adapt_filter_intra[0][0], probsfile, 2, cts_each_dim,
      "static const aom_cdf_prob "
      "default_adapt_filter_intra_cdfs[BLOCK_SIZES_ALL][CDF_SIZE(2)]");
#endif  // CONFIG_ADAPT_FILTER_INTRA

  /* restoration type */
  cts_each_dim[0] = RESTORE_SWITCHABLE_TYPES;
  optimize_cdf_table(&fc.switchable_restore[0], probsfile, 1, cts_each_dim,
                     "static const aom_cdf_prob default_switchable_restore_cdf"
                     "[CDF_SIZE(RESTORE_SWITCHABLE_TYPES)]");

  cts_each_dim[0] = 2;
  optimize_cdf_table(&fc.wiener_restore[0], probsfile, 1, cts_each_dim,
                     "static const aom_cdf_prob default_wiener_restore_cdf"
                     "[CDF_SIZE(2)]");

  cts_each_dim[0] = 2;
  optimize_cdf_table(&fc.sgrproj_restore[0], probsfile, 1, cts_each_dim,
                     "static const aom_cdf_prob default_sgrproj_restore_cdf"
                     "[CDF_SIZE(2)]");

  /* intra tx size */
  cts_each_dim[0] = MAX_TX_CATS;
  cts_each_dim[1] = TX_SIZE_CONTEXTS;
  cts_each_dim[2] = MAX_TX_DEPTH + 1;
  int intra_tx_sizes_each_ctx[MAX_TX_CATS] = { 2, 3, 3, 3 };
  optimize_cdf_table_var_modes_3d(
      &fc.intra_tx_size[0][0][0], probsfile, 3, cts_each_dim,
      intra_tx_sizes_each_ctx,
      "static const aom_cdf_prob default_tx_size_cdf"
      "[MAX_TX_CATS][TX_SIZE_CONTEXTS][CDF_SIZE(MAX_TX_DEPTH + 1)]");

  /* transform coding */
  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = TX_SIZES;
  cts_each_dim[2] = TXB_SKIP_CONTEXTS;
  cts_each_dim[3] = 2;
  optimize_cdf_table(&fc.txb_skip[0][0][0][0], probsfile, 4, cts_each_dim,
                     "static const aom_cdf_prob "
                     "av1_default_txb_skip_cdfs[TOKEN_CDF_Q_CTXS][TX_SIZES]"
                     "[TXB_SKIP_CONTEXTS][CDF_SIZE(2)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = TX_SIZES;
  cts_each_dim[2] = PLANE_TYPES;
  cts_each_dim[3] = EOB_COEF_CONTEXTS;
  cts_each_dim[4] = 2;
  optimize_cdf_table(
      &fc.eob_extra[0][0][0][0][0], probsfile, 5, cts_each_dim,
      "static const aom_cdf_prob av1_default_eob_extra_cdfs "
      "[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS]"
      "[CDF_SIZE(2)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = 2;
  cts_each_dim[3] = 5;
  optimize_cdf_table(&fc.eob_multi16[0][0][0][0], probsfile, 4, cts_each_dim,
                     "static const aom_cdf_prob av1_default_eob_multi16_cdfs"
                     "[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(5)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = 2;
  cts_each_dim[3] = 6;
  optimize_cdf_table(&fc.eob_multi32[0][0][0][0], probsfile, 4, cts_each_dim,
                     "static const aom_cdf_prob av1_default_eob_multi32_cdfs"
                     "[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(6)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = 2;
  cts_each_dim[3] = 7;
  optimize_cdf_table(&fc.eob_multi64[0][0][0][0], probsfile, 4, cts_each_dim,
                     "static const aom_cdf_prob av1_default_eob_multi64_cdfs"
                     "[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(7)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = 2;
  cts_each_dim[3] = 8;
  optimize_cdf_table(&fc.eob_multi128[0][0][0][0], probsfile, 4, cts_each_dim,
                     "static const aom_cdf_prob av1_default_eob_multi128_cdfs"
                     "[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(8)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = 2;
  cts_each_dim[3] = 9;
  optimize_cdf_table(&fc.eob_multi256[0][0][0][0], probsfile, 4, cts_each_dim,
                     "static const aom_cdf_prob av1_default_eob_multi256_cdfs"
                     "[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(9)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = 2;
  cts_each_dim[3] = 10;
  optimize_cdf_table(&fc.eob_multi512[0][0][0][0], probsfile, 4, cts_each_dim,
                     "static const aom_cdf_prob av1_default_eob_multi512_cdfs"
                     "[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(10)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = PLANE_TYPES;
  cts_each_dim[2] = 2;
  cts_each_dim[3] = 11;
  optimize_cdf_table(&fc.eob_multi1024[0][0][0][0], probsfile, 4, cts_each_dim,
                     "static const aom_cdf_prob av1_default_eob_multi1024_cdfs"
                     "[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(11)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = TX_SIZES;
  cts_each_dim[2] = PLANE_TYPES;
  cts_each_dim[3] = LEVEL_CONTEXTS;
  cts_each_dim[4] = BR_CDF_SIZE;
  optimize_cdf_table(&fc.coeff_lps_multi[0][0][0][0][0], probsfile, 5,
                     cts_each_dim,
                     "static const aom_cdf_prob "
                     "av1_default_coeff_lps_multi_cdfs[TOKEN_CDF_Q_CTXS]"
                     "[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS]"
                     "[CDF_SIZE(BR_CDF_SIZE)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = TX_SIZES;
  cts_each_dim[2] = PLANE_TYPES;
  cts_each_dim[3] = SIG_COEF_CONTEXTS;
  cts_each_dim[4] = NUM_BASE_LEVELS + 2;
  optimize_cdf_table(
      &fc.coeff_base_multi[0][0][0][0][0], probsfile, 5, cts_each_dim,
      "static const aom_cdf_prob av1_default_coeff_base_multi_cdfs"
      "[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS]"
      "[CDF_SIZE(NUM_BASE_LEVELS + 2)]");

  cts_each_dim[0] = TOKEN_CDF_Q_CTXS;
  cts_each_dim[1] = TX_SIZES;
  cts_each_dim[2] = PLANE_TYPES;
  cts_each_dim[3] = SIG_COEF_CONTEXTS_EOB;
  cts_each_dim[4] = NUM_BASE_LEVELS + 1;
  optimize_cdf_table(
      &fc.coeff_base_eob_multi[0][0][0][0][0], probsfile, 5, cts_each_dim,
      "static const aom_cdf_prob av1_default_coeff_base_eob_multi_cdfs"
      "[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS_EOB]"
      "[CDF_SIZE(NUM_BASE_LEVELS + 1)]");

  fclose(statsfile);
  fclose(logfile);
  fclose(probsfile);

  return 0;
}
