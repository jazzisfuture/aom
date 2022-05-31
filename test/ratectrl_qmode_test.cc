/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <array>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "av1/ratectrl_qmode.h"
#include "av1/reference_manager.h"
#include "test/mock_ratectrl_qmode.h"
#include "test/video_source.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

constexpr int kRefFrameTableSize = 7;

// Reads a whitespace-delimited string from stream, and parses it as a double.
// Returns an empty string if the entire string was successfully parsed as a
// double, or an error messaage if not.
std::string ReadDouble(std::istream &stream, double *value) {
  std::string word;
  stream >> word;
  if (word.empty()) {
    return "Unexpectedly reached end of input";
  }
  char *end;
  *value = std::strtod(word.c_str(), &end);
  if (*end != '\0') {
    return "Unexpected characters found: " + word;
  }
  return "";
}

void ReadFirstpassInfo(const std::string &filename,
                       aom::FirstpassInfo *firstpass_info) {
  // These golden files are generated by the following command line:
  // ./aomenc --width=352 --height=288 --fps=30/1 --limit=250 --codec=av1
  // --cpu-used=3 --end-usage=q --cq-level=36 --threads=0 --profile=0
  // --lag-in-frames=35 --min-q=0 --max-q=63 --auto-alt-ref=1 --passes=2
  // --kf-max-dist=160 --kf-min-dist=0 --drop-frame=0
  // --static-thresh=0 --minsection-pct=0 --maxsection-pct=2000
  // --arnr-maxframes=7
  // --arnr-strength=5 --sharpness=0 --undershoot-pct=100 --overshoot-pct=100
  // --frame-parallel=0
  // --tile-columns=0 -o output.webm hantro_collage_w352h288.yuv
  // First pass stats are written out in av1_get_second_pass_params right after
  // calculate_gf_length.
  std::string path = libaom_test::GetDataPath() + "/" + filename;
  std::ifstream firstpass_stats_file(path);
  ASSERT_TRUE(firstpass_stats_file.good())
      << "Error opening " << path << ": " << std::strerror(errno);
  firstpass_info->num_mbs_16x16 = (352 / 16 + 1) * (288 / 16 + 1);
  std::string newline;
  while (std::getline(firstpass_stats_file, newline)) {
    std::istringstream iss(newline);
    FIRSTPASS_STATS firstpass_stats_input = {};
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.frame), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.weight), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.intra_error), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.frame_avg_wavelet_energy),
              "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.coded_error), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.sr_coded_error), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.pcnt_inter), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.pcnt_motion), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.pcnt_second_ref), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.pcnt_neutral), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.intra_skip_pct), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.inactive_zone_rows), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.inactive_zone_cols), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.MVr), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.mvr_abs), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.MVc), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.mvc_abs), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.MVrv), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.MVcv), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.mv_in_out_count), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.new_mv_count), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.duration), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.count), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.raw_error_stdev), "");
    iss >> firstpass_stats_input.is_flash;
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.noise_var), "");
    ASSERT_EQ(ReadDouble(iss, &firstpass_stats_input.cor_coeff), "");
    ASSERT_TRUE(iss.eof()) << "Too many fields on line "
                           << firstpass_info->stats_list.size() + 1 << "\n"
                           << newline;
    firstpass_info->stats_list.push_back(firstpass_stats_input);
  }
}

}  // namespace

namespace aom {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Return;

constexpr double kErrorEpsilon = 0.000001;

void TestGopDisplayOrder(const GopStruct &gop_struct) {
  // Test whether show frames' order indices are sequential
  int expected_order_idx = 0;
  int expected_show_frame_count = 0;
  for (const auto &gop_frame : gop_struct.gop_frame_list) {
    if (gop_frame.is_show_frame) {
      EXPECT_EQ(gop_frame.order_idx, expected_order_idx);
      expected_order_idx++;
      expected_show_frame_count++;
    }
  }
  EXPECT_EQ(gop_struct.show_frame_count, expected_show_frame_count);
}

void TestGopGlobalOrderIdx(const GopStruct &gop_struct,
                           int global_order_idx_offset) {
  // Test whether show frames' global order indices are sequential
  EXPECT_EQ(gop_struct.global_order_idx_offset, global_order_idx_offset);
  int expected_global_order_idx = global_order_idx_offset;
  for (const auto &gop_frame : gop_struct.gop_frame_list) {
    if (gop_frame.is_show_frame) {
      EXPECT_EQ(gop_frame.global_order_idx, expected_global_order_idx);
      expected_global_order_idx++;
    }
  }
}

void TestGopGlobalCodingIdx(const GopStruct &gop_struct,
                            int global_coding_idx_offset) {
  EXPECT_EQ(gop_struct.global_coding_idx_offset, global_coding_idx_offset);
  for (const auto &gop_frame : gop_struct.gop_frame_list) {
    EXPECT_EQ(gop_frame.global_coding_idx,
              global_coding_idx_offset + gop_frame.coding_idx);
  }
}

void TestColocatedShowFrame(const GopStruct &gop_struct) {
  // Test whether each non show frame has a colocated show frame
  int gop_size = static_cast<int>(gop_struct.gop_frame_list.size());
  for (int gop_idx = 0; gop_idx < gop_size; ++gop_idx) {
    auto &gop_frame = gop_struct.gop_frame_list[gop_idx];
    if (gop_frame.is_show_frame == 0) {
      bool found_colocated_ref_frame = false;
      for (int i = gop_idx + 1; i < gop_size; ++i) {
        auto &next_gop_frame = gop_struct.gop_frame_list[i];
        if (gop_frame.order_idx == next_gop_frame.order_idx) {
          found_colocated_ref_frame = true;
          EXPECT_EQ(gop_frame.update_ref_idx, next_gop_frame.colocated_ref_idx);
          EXPECT_TRUE(next_gop_frame.is_show_frame);
        }
        if (gop_frame.update_ref_idx == next_gop_frame.update_ref_idx) {
          break;
        }
      }
      EXPECT_TRUE(found_colocated_ref_frame);
    }
  }
}

void TestLayerDepth(const GopStruct &gop_struct, int max_layer_depth) {
  int gop_size = static_cast<int>(gop_struct.gop_frame_list.size());
  for (int gop_idx = 0; gop_idx < gop_size; ++gop_idx) {
    const auto &gop_frame = gop_struct.gop_frame_list[gop_idx];
    if (gop_frame.is_key_frame) {
      EXPECT_EQ(gop_frame.layer_depth, 0);
    }

    if (gop_frame.is_arf_frame) {
      EXPECT_LT(gop_frame.layer_depth, max_layer_depth);
    }

    if (!gop_frame.is_key_frame && !gop_frame.is_arf_frame) {
      EXPECT_EQ(gop_frame.layer_depth, max_layer_depth);
    }
  }
}

void TestArfInterval(const GopStruct &gop_struct) {
  std::vector<int> arf_order_idx_list;
  for (const auto &gop_frame : gop_struct.gop_frame_list) {
    if (gop_frame.is_arf_frame) {
      arf_order_idx_list.push_back(gop_frame.order_idx);
    }
  }
  std::sort(arf_order_idx_list.begin(), arf_order_idx_list.end());
  int arf_count = static_cast<int>(arf_order_idx_list.size());
  for (int i = 1; i < arf_count; ++i) {
    int arf_interval = arf_order_idx_list[i] - arf_order_idx_list[i - 1];
    EXPECT_GE(arf_interval, kMinArfInterval);
  }
}

TEST(RateControlQModeTest, ConstructGopARF) {
  int show_frame_count = 16;
  const bool has_key_frame = false;
  const int global_coding_idx_offset = 5;
  const int global_order_idx_offset = 20;
  RefFrameManager ref_frame_manager(kRefFrameTableSize);
  GopStruct gop_struct =
      ConstructGop(&ref_frame_manager, show_frame_count, has_key_frame,
                   global_coding_idx_offset, global_order_idx_offset);
  EXPECT_EQ(gop_struct.show_frame_count, show_frame_count);
  TestGopDisplayOrder(gop_struct);
  TestGopGlobalOrderIdx(gop_struct, global_order_idx_offset);
  TestGopGlobalCodingIdx(gop_struct, global_coding_idx_offset);
  TestColocatedShowFrame(gop_struct);
  const int max_layer_depth =
      ref_frame_manager.ForwardMaxSize() + kLayerDepthOffset;
  TestLayerDepth(gop_struct, max_layer_depth);
  TestArfInterval(gop_struct);
}

TEST(RateControlQModeTest, ConstructGopKey) {
  const int show_frame_count = 16;
  const int has_key_frame = 1;
  const int global_coding_idx_offset = 10;
  const int global_order_idx_offset = 8;
  RefFrameManager ref_frame_manager(kRefFrameTableSize);
  GopStruct gop_struct =
      ConstructGop(&ref_frame_manager, show_frame_count, has_key_frame,
                   global_coding_idx_offset, global_order_idx_offset);
  EXPECT_EQ(gop_struct.show_frame_count, show_frame_count);
  TestGopDisplayOrder(gop_struct);
  TestGopGlobalOrderIdx(gop_struct, global_order_idx_offset);
  TestGopGlobalCodingIdx(gop_struct, global_coding_idx_offset);
  TestColocatedShowFrame(gop_struct);
  const int max_layer_depth =
      ref_frame_manager.ForwardMaxSize() + kLayerDepthOffset;
  TestLayerDepth(gop_struct, max_layer_depth);
  TestArfInterval(gop_struct);
}

static TplBlockStats CreateToyTplBlockStats(int h, int w, int r, int c,
                                            int intra_cost, int inter_cost) {
  TplBlockStats tpl_block_stats = {};
  tpl_block_stats.height = h;
  tpl_block_stats.width = w;
  tpl_block_stats.row = r;
  tpl_block_stats.col = c;
  tpl_block_stats.intra_cost = intra_cost;
  tpl_block_stats.inter_cost = inter_cost;
  tpl_block_stats.ref_frame_index = { -1, -1 };
  return tpl_block_stats;
}

static TplFrameStats CreateToyTplFrameStatsWithDiffSizes(int min_block_size,
                                                         int max_block_size) {
  TplFrameStats frame_stats;
  const int max_h = max_block_size;
  const int max_w = max_h;
  const int count = max_block_size / min_block_size;
  frame_stats.min_block_size = min_block_size;
  frame_stats.frame_height = max_h * count;
  frame_stats.frame_width = max_w * count;
  for (int i = 0; i < count; ++i) {
    for (int j = 0; j < count; ++j) {
      int h = max_h >> i;
      int w = max_w >> j;
      for (int u = 0; u * h < max_h; ++u) {
        for (int v = 0; v * w < max_w; ++v) {
          int r = max_h * i + h * u;
          int c = max_w * j + w * v;
          int intra_cost = std::rand() % 16;
          TplBlockStats block_stats =
              CreateToyTplBlockStats(h, w, r, c, intra_cost, 0);
          frame_stats.block_stats_list.push_back(block_stats);
        }
      }
    }
  }
  return frame_stats;
}

static void AugmentTplFrameStatsWithRefFrames(
    TplFrameStats *tpl_frame_stats,
    const std::array<int, kBlockRefCount> &ref_frame_index) {
  for (auto &block_stats : tpl_frame_stats->block_stats_list) {
    block_stats.ref_frame_index = ref_frame_index;
  }
}
static void AugmentTplFrameStatsWithMotionVector(
    TplFrameStats *tpl_frame_stats,
    const std::array<MotionVector, kBlockRefCount> &mv) {
  for (auto &block_stats : tpl_frame_stats->block_stats_list) {
    block_stats.mv = mv;
  }
}

static RefFrameTable CreateToyRefFrameTable(int frame_count) {
  RefFrameTable ref_frame_table(kRefFrameTableSize);
  EXPECT_LE(frame_count, kRefFrameTableSize);
  for (int i = 0; i < frame_count; ++i) {
    ref_frame_table[i] =
        GopFrameBasic(0, 0, i, i, 0, GopFrameType::kRegularLeaf);
  }
  for (int i = frame_count; i < kRefFrameTableSize; ++i) {
    ref_frame_table[i] = GopFrameInvalid();
  }
  return ref_frame_table;
}

static MotionVector CreateFullpelMv(int row, int col) {
  return { row, col, 0 };
}

double TplFrameStatsAccumulateIntraCost(const TplFrameStats &frame_stats) {
  double sum = 0;
  for (auto &block_stats : frame_stats.block_stats_list) {
    sum += block_stats.intra_cost;
  }
  return sum;
}

TEST(RateControlQModeTest, CreateTplFrameDepStats) {
  TplFrameStats frame_stats = CreateToyTplFrameStatsWithDiffSizes(8, 16);
  TplFrameDepStats frame_dep_stats =
      CreateTplFrameDepStatsWithoutPropagation(frame_stats);
  EXPECT_EQ(frame_stats.min_block_size, frame_dep_stats.unit_size);
  const int unit_rows = static_cast<int>(frame_dep_stats.unit_stats.size());
  const int unit_cols = static_cast<int>(frame_dep_stats.unit_stats[0].size());
  EXPECT_EQ(frame_stats.frame_height, unit_rows * frame_dep_stats.unit_size);
  EXPECT_EQ(frame_stats.frame_width, unit_cols * frame_dep_stats.unit_size);
  const double intra_cost_sum =
      TplFrameDepStatsAccumulateIntraCost(frame_dep_stats);

  const double expected_intra_cost_sum =
      TplFrameStatsAccumulateIntraCost(frame_stats);
  EXPECT_NEAR(intra_cost_sum, expected_intra_cost_sum, kErrorEpsilon);
}

TEST(RateControlQModeTest, GetBlockOverlapArea) {
  const int size = 8;
  const int r0 = 8;
  const int c0 = 9;
  std::vector<int> r1 = { 8, 10, 16, 10, 8, 100 };
  std::vector<int> c1 = { 9, 12, 17, 5, 100, 9 };
  std::vector<int> ref_overlap = { 64, 30, 0, 24, 0, 0 };
  for (int i = 0; i < static_cast<int>(r1.size()); ++i) {
    const int overlap0 = GetBlockOverlapArea(r0, c0, r1[i], c1[i], size);
    const int overlap1 = GetBlockOverlapArea(r1[i], c1[i], r0, c0, size);
    EXPECT_EQ(overlap0, ref_overlap[i]);
    EXPECT_EQ(overlap1, ref_overlap[i]);
  }
}

TEST(RateControlQModeTest, TplBlockStatsToDepStats) {
  const int intra_cost = 100;
  const int inter_cost = 120;
  const int unit_count = 2;
  TplBlockStats block_stats =
      CreateToyTplBlockStats(8, 4, 0, 0, intra_cost, inter_cost);
  TplUnitDepStats unit_stats = TplBlockStatsToDepStats(block_stats, unit_count);
  double expected_intra_cost = intra_cost * 1.0 / unit_count;
  EXPECT_NEAR(unit_stats.intra_cost, expected_intra_cost, kErrorEpsilon);
  // When inter_cost >= intra_cost in block_stats, in unit_stats,
  // the inter_cost will be modified so that it's upper-bounded by intra_cost.
  EXPECT_LE(unit_stats.inter_cost, unit_stats.intra_cost);
}

TEST(RateControlQModeTest, TplFrameDepStatsPropagateSingleZeroMotion) {
  // cur frame with coding_idx 1 use ref frame with coding_idx 0
  const std::array<int, kBlockRefCount> ref_frame_index = { 0, -1 };
  TplFrameStats frame_stats = CreateToyTplFrameStatsWithDiffSizes(8, 16);
  AugmentTplFrameStatsWithRefFrames(&frame_stats, ref_frame_index);

  TplGopDepStats gop_dep_stats;
  const int frame_count = 2;
  // ref frame with coding_idx 0
  TplFrameDepStats frame_dep_stats0 =
      CreateTplFrameDepStats(frame_stats.frame_height, frame_stats.frame_width,
                             frame_stats.min_block_size);
  gop_dep_stats.frame_dep_stats_list.push_back(frame_dep_stats0);

  // cur frame with coding_idx 1
  const TplFrameDepStats frame_dep_stats1 =
      CreateTplFrameDepStatsWithoutPropagation(frame_stats);
  gop_dep_stats.frame_dep_stats_list.push_back(frame_dep_stats1);

  const RefFrameTable ref_frame_table = CreateToyRefFrameTable(frame_count);
  TplFrameDepStatsPropagate(/*coding_idx=*/1, ref_frame_table, &gop_dep_stats);

  // cur frame with coding_idx 1
  const double expected_propagation_sum =
      TplFrameStatsAccumulateIntraCost(frame_stats);

  // ref frame with coding_idx 0
  const double propagation_sum =
      TplFrameDepStatsAccumulate(gop_dep_stats.frame_dep_stats_list[0]);

  // The propagation_sum between coding_idx 0 and coding_idx 1 should be equal
  // because every block in cur frame has zero motion, use ref frame with
  // coding_idx 0 for prediction, and ref frame itself is empty.
  EXPECT_NEAR(propagation_sum, expected_propagation_sum, kErrorEpsilon);
}

TEST(RateControlQModeTest, TplFrameDepStatsPropagateCompoundZeroMotion) {
  // cur frame with coding_idx 2 use two ref frames with coding_idx 0 and 1
  const std::array<int, kBlockRefCount> ref_frame_index = { 0, 1 };
  TplFrameStats frame_stats = CreateToyTplFrameStatsWithDiffSizes(8, 16);
  AugmentTplFrameStatsWithRefFrames(&frame_stats, ref_frame_index);

  TplGopDepStats gop_dep_stats;
  const int frame_count = 3;
  // ref frame with coding_idx 0
  const TplFrameDepStats frame_dep_stats0 =
      CreateTplFrameDepStats(frame_stats.frame_height, frame_stats.frame_width,
                             frame_stats.min_block_size);
  gop_dep_stats.frame_dep_stats_list.push_back(frame_dep_stats0);

  // ref frame with coding_idx 1
  const TplFrameDepStats frame_dep_stats1 =
      CreateTplFrameDepStats(frame_stats.frame_height, frame_stats.frame_width,
                             frame_stats.min_block_size);
  gop_dep_stats.frame_dep_stats_list.push_back(frame_dep_stats1);

  // cur frame with coding_idx 2
  const TplFrameDepStats frame_dep_stats2 =
      CreateTplFrameDepStatsWithoutPropagation(frame_stats);
  gop_dep_stats.frame_dep_stats_list.push_back(frame_dep_stats2);

  const RefFrameTable ref_frame_table = CreateToyRefFrameTable(frame_count);
  TplFrameDepStatsPropagate(/*coding_idx=*/2, ref_frame_table, &gop_dep_stats);

  // cur frame with coding_idx 1
  const double expected_ref_sum = TplFrameStatsAccumulateIntraCost(frame_stats);

  // ref frame with coding_idx 0
  const double cost_sum0 =
      TplFrameDepStatsAccumulate(gop_dep_stats.frame_dep_stats_list[0]);
  EXPECT_NEAR(cost_sum0, expected_ref_sum * 0.5, kErrorEpsilon);

  // ref frame with coding_idx 1
  const double cost_sum1 =
      TplFrameDepStatsAccumulate(gop_dep_stats.frame_dep_stats_list[1]);
  EXPECT_NEAR(cost_sum1, expected_ref_sum * 0.5, kErrorEpsilon);
}

TEST(RateControlQModeTest, TplFrameDepStatsPropagateSingleWithMotion) {
  // cur frame with coding_idx 1 use ref frame with coding_idx 0
  const std::array<int, kBlockRefCount> ref_frame_index = { 0, -1 };
  const int min_block_size = 8;
  TplFrameStats frame_stats =
      CreateToyTplFrameStatsWithDiffSizes(min_block_size, min_block_size * 2);
  AugmentTplFrameStatsWithRefFrames(&frame_stats, ref_frame_index);

  const int mv_row = min_block_size / 2;
  const int mv_col = min_block_size / 4;
  const double r_ratio = 1.0 / 2;
  const double c_ratio = 1.0 / 4;
  std::array<MotionVector, kBlockRefCount> mv;
  mv[0] = CreateFullpelMv(mv_row, mv_col);
  mv[1] = CreateFullpelMv(0, 0);
  AugmentTplFrameStatsWithMotionVector(&frame_stats, mv);

  TplGopDepStats gop_dep_stats;
  const int frame_count = 2;
  // ref frame with coding_idx 0
  gop_dep_stats.frame_dep_stats_list.push_back(
      CreateTplFrameDepStats(frame_stats.frame_height, frame_stats.frame_width,
                             frame_stats.min_block_size));

  // cur frame with coding_idx 1
  gop_dep_stats.frame_dep_stats_list.push_back(
      CreateTplFrameDepStatsWithoutPropagation(frame_stats));

  const RefFrameTable ref_frame_table = CreateToyRefFrameTable(frame_count);
  TplFrameDepStatsPropagate(/*coding_idx=*/1, ref_frame_table, &gop_dep_stats);

  const auto &dep_stats0 = gop_dep_stats.frame_dep_stats_list[0];
  const auto &dep_stats1 = gop_dep_stats.frame_dep_stats_list[1];
  const int unit_rows = static_cast<int>(dep_stats0.unit_stats.size());
  const int unit_cols = static_cast<int>(dep_stats0.unit_stats[0].size());
  for (int r = 0; r < unit_rows; ++r) {
    for (int c = 0; c < unit_cols; ++c) {
      double ref_value = 0;
      ref_value += (1 - r_ratio) * (1 - c_ratio) *
                   dep_stats1.unit_stats[r][c].intra_cost;
      if (r - 1 >= 0) {
        ref_value += r_ratio * (1 - c_ratio) *
                     dep_stats1.unit_stats[r - 1][c].intra_cost;
      }
      if (c - 1 >= 0) {
        ref_value += (1 - r_ratio) * c_ratio *
                     dep_stats1.unit_stats[r][c - 1].intra_cost;
      }
      if (r - 1 >= 0 && c - 1 >= 0) {
        ref_value +=
            r_ratio * c_ratio * dep_stats1.unit_stats[r - 1][c - 1].intra_cost;
      }
      EXPECT_NEAR(dep_stats0.unit_stats[r][c].propagation_cost, ref_value,
                  kErrorEpsilon);
    }
  }
}

TEST(RateControlQModeTest, ComputeTplGopDepStats) {
  TplGopStats tpl_gop_stats;
  std::vector<RefFrameTable> ref_frame_table_list;
  for (int i = 0; i < 3; i++) {
    // Use the previous frame as reference
    const std::array<int, kBlockRefCount> ref_frame_index = { i - 1, -1 };
    int min_block_size = 8;
    TplFrameStats frame_stats =
        CreateToyTplFrameStatsWithDiffSizes(min_block_size, min_block_size * 2);
    AugmentTplFrameStatsWithRefFrames(&frame_stats, ref_frame_index);
    tpl_gop_stats.frame_stats_list.push_back(frame_stats);

    ref_frame_table_list.push_back(CreateToyRefFrameTable(i));
  }
  const TplGopDepStats &gop_dep_stats =
      ComputeTplGopDepStats(tpl_gop_stats, ref_frame_table_list);

  double expected_sum = 0;
  for (int i = 2; i >= 0; i--) {
    // Due to the linear propagation with zero motion, we can accumulate the
    // frame_stats intra_cost and use it as expected sum for dependency stats
    expected_sum +=
        TplFrameStatsAccumulateIntraCost(tpl_gop_stats.frame_stats_list[i]);
    const double sum =
        TplFrameDepStatsAccumulate(gop_dep_stats.frame_dep_stats_list[i]);
    EXPECT_NEAR(sum, expected_sum, kErrorEpsilon);
    break;
  }
}

TEST(RefFrameManagerTest, GetRefFrameCount) {
  const std::vector<int> order_idx_list = { 0, 4, 2, 1, 2, 3, 4 };
  const std::vector<GopFrameType> type_list = {
    GopFrameType::kRegularKey,      GopFrameType::kRegularArf,
    GopFrameType::kIntermediateArf, GopFrameType::kRegularLeaf,
    GopFrameType::kShowExisting,    GopFrameType::kRegularLeaf,
    GopFrameType::kOverlay
  };
  RefFrameManager ref_manager(kRefFrameTableSize);
  int coding_idx = 0;
  const int first_leaf_idx = 3;
  EXPECT_EQ(type_list[first_leaf_idx], GopFrameType::kRegularLeaf);
  // update reference frame until we see the first kRegularLeaf frame
  for (; coding_idx <= first_leaf_idx; ++coding_idx) {
    GopFrame gop_frame = GopFrameBasic(
        0, 0, coding_idx, order_idx_list[coding_idx], 0, type_list[coding_idx]);
    ref_manager.UpdateRefFrameTable(&gop_frame);
  }
  EXPECT_EQ(ref_manager.GetRefFrameCount(), 4);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kForward), 2);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kBackward), 1);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kLast), 1);
  EXPECT_EQ(ref_manager.CurGlobalOrderIdx(), 1);

  // update reference frame until we see the first kShowExisting frame
  const int first_show_existing_idx = 4;
  EXPECT_EQ(type_list[first_show_existing_idx], GopFrameType::kShowExisting);
  for (; coding_idx <= first_show_existing_idx; ++coding_idx) {
    GopFrame gop_frame = GopFrameBasic(
        0, 0, coding_idx, order_idx_list[coding_idx], 0, type_list[coding_idx]);
    ref_manager.UpdateRefFrameTable(&gop_frame);
  }
  EXPECT_EQ(ref_manager.GetRefFrameCount(), 4);
  EXPECT_EQ(ref_manager.CurGlobalOrderIdx(), 2);
  // After the first kShowExisting, the kIntermediateArf should be moved from
  // kForward to kLast due to the cur_global_order_idx_ update
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kForward), 1);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kBackward), 1);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kLast), 2);

  const int second_leaf_idx = 5;
  EXPECT_EQ(type_list[second_leaf_idx], GopFrameType::kRegularLeaf);
  for (; coding_idx <= second_leaf_idx; ++coding_idx) {
    GopFrame gop_frame = GopFrameBasic(
        0, 0, coding_idx, order_idx_list[coding_idx], 0, type_list[coding_idx]);
    ref_manager.UpdateRefFrameTable(&gop_frame);
  }
  EXPECT_EQ(ref_manager.GetRefFrameCount(), 5);
  EXPECT_EQ(ref_manager.CurGlobalOrderIdx(), 3);
  // An additional kRegularLeaf frame is added into kLast
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kForward), 1);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kBackward), 1);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kLast), 3);

  const int first_overlay_idx = 6;
  EXPECT_EQ(type_list[first_overlay_idx], GopFrameType::kOverlay);
  for (; coding_idx <= first_overlay_idx; ++coding_idx) {
    GopFrame gop_frame = GopFrameBasic(
        0, 0, coding_idx, order_idx_list[coding_idx], 0, type_list[coding_idx]);
    ref_manager.UpdateRefFrameTable(&gop_frame);
  }

  EXPECT_EQ(ref_manager.GetRefFrameCount(), 5);
  EXPECT_EQ(ref_manager.CurGlobalOrderIdx(), 4);
  // After the kOverlay, the kRegularArf should be moved from
  // kForward to kBackward due to the cur_global_order_idx_ update
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kForward), 0);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kBackward), 2);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kLast), 3);
}

void TestRefFrameManagerPriority(const RefFrameManager &ref_manager,
                                 RefUpdateType type) {
  int ref_count = ref_manager.GetRefFrameCountByType(type);
  int prev_global_order_idx = ref_manager.CurGlobalOrderIdx();
  // The lower the priority is, the closer the gop_frame.global_order_idx should
  // be with cur_global_order_idx_
  for (int priority = 0; priority < ref_count; ++priority) {
    GopFrame gop_frame = ref_manager.GetRefFrameByPriority(type, priority);
    EXPECT_EQ(gop_frame.is_valid, true);
    if (type == RefUpdateType::kForward) {
      EXPECT_GE(gop_frame.global_order_idx, prev_global_order_idx);
    } else {
      EXPECT_LE(gop_frame.global_order_idx, prev_global_order_idx);
    }
    prev_global_order_idx = gop_frame.global_order_idx;
  }
  GopFrame gop_frame =
      ref_manager.GetRefFrameByPriority(RefUpdateType::kForward, ref_count);
  EXPECT_EQ(gop_frame.is_valid, false);
}

TEST(RefFrameManagerTest, GetRefFrameByPriority) {
  const std::vector<int> order_idx_list = { 0, 4, 2, 1, 2, 3, 4 };
  const std::vector<GopFrameType> type_list = {
    GopFrameType::kRegularKey,      GopFrameType::kRegularArf,
    GopFrameType::kIntermediateArf, GopFrameType::kRegularLeaf,
    GopFrameType::kShowExisting,    GopFrameType::kRegularLeaf,
    GopFrameType::kOverlay
  };
  RefFrameManager ref_manager(kRefFrameTableSize);
  int coding_idx = 0;
  const int first_leaf_idx = 3;
  EXPECT_EQ(type_list[first_leaf_idx], GopFrameType::kRegularLeaf);
  // update reference frame until we see the first kRegularLeaf frame
  for (; coding_idx <= first_leaf_idx; ++coding_idx) {
    GopFrame gop_frame = GopFrameBasic(
        0, 0, coding_idx, order_idx_list[coding_idx], 0, type_list[coding_idx]);
    ref_manager.UpdateRefFrameTable(&gop_frame);
  }
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kForward), 2);
  TestRefFrameManagerPriority(ref_manager, RefUpdateType::kForward);

  const int first_overlay_idx = 6;
  EXPECT_EQ(type_list[first_overlay_idx], GopFrameType::kOverlay);
  for (; coding_idx <= first_overlay_idx; ++coding_idx) {
    GopFrame gop_frame = GopFrameBasic(
        0, 0, coding_idx, order_idx_list[coding_idx], 0, type_list[coding_idx]);
    ref_manager.UpdateRefFrameTable(&gop_frame);
  }

  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kBackward), 2);
  TestRefFrameManagerPriority(ref_manager, RefUpdateType::kBackward);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kLast), 3);
  TestRefFrameManagerPriority(ref_manager, RefUpdateType::kLast);
}

TEST(RefFrameManagerTest, GetRefFrameListByPriority) {
  const std::vector<int> order_idx_list = { 0, 4, 2, 1 };
  const int frame_count = static_cast<int>(order_idx_list.size());
  const std::vector<GopFrameType> type_list = { GopFrameType::kRegularKey,
                                                GopFrameType::kRegularArf,
                                                GopFrameType::kIntermediateArf,
                                                GopFrameType::kRegularLeaf };
  RefFrameManager ref_manager(kRefFrameTableSize);
  for (int coding_idx = 0; coding_idx < frame_count; ++coding_idx) {
    GopFrame gop_frame = GopFrameBasic(
        0, 0, coding_idx, order_idx_list[coding_idx], 0, type_list[coding_idx]);
    ref_manager.UpdateRefFrameTable(&gop_frame);
  }
  EXPECT_EQ(ref_manager.GetRefFrameCount(), frame_count);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kForward), 2);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kBackward), 1);
  EXPECT_EQ(ref_manager.GetRefFrameCountByType(RefUpdateType::kLast), 1);
  std::vector<ReferenceFrame> ref_frame_list =
      ref_manager.GetRefFrameListByPriority();
  EXPECT_EQ(ref_frame_list.size(), order_idx_list.size());
  std::vector<int> expected_global_order_idx = { 2, 0, 1, 4 };
  std::vector<ReferenceName> expected_names = { ReferenceName::kBwdrefFrame,
                                                ReferenceName::kGoldenFrame,
                                                ReferenceName::kLastFrame,
                                                ReferenceName::kAltref2Frame };
  for (size_t i = 0; i < ref_frame_list.size(); ++i) {
    ReferenceFrame &ref_frame = ref_frame_list[i];
    GopFrame gop_frame = ref_manager.GetRefFrameByIndex(ref_frame.index);
    EXPECT_EQ(gop_frame.global_order_idx, expected_global_order_idx[i]);
    EXPECT_EQ(ref_frame.name, expected_names[i]);
  }
}

TEST(RefFrameManagerTest, GetPrimaryRefFrame) {
  const std::vector<int> order_idx_list = { 0, 4, 2, 1 };
  const int frame_count = static_cast<int>(order_idx_list.size());
  const std::vector<GopFrameType> type_list = { GopFrameType::kRegularKey,
                                                GopFrameType::kRegularArf,
                                                GopFrameType::kIntermediateArf,
                                                GopFrameType::kRegularLeaf };
  const std::vector<int> layer_depth_list = { 0, 2, 4, 6 };
  RefFrameManager ref_manager(kRefFrameTableSize);
  for (int coding_idx = 0; coding_idx < frame_count; ++coding_idx) {
    GopFrame gop_frame =
        GopFrameBasic(0, 0, coding_idx, order_idx_list[coding_idx],
                      layer_depth_list[coding_idx], type_list[coding_idx]);
    ref_manager.UpdateRefFrameTable(&gop_frame);
  }

  for (int i = 0; i < frame_count; ++i) {
    // Test frame that share the same layer depth with a reference frame
    int layer_depth = layer_depth_list[i];
    // Set different frame type
    GopFrameType type = type_list[(i + 1) % frame_count];
    GopFrame gop_frame = GopFrameBasic(0, 0, 0, 0, layer_depth, type);
    ReferenceFrame ref_frame = ref_manager.GetPrimaryRefFrame(gop_frame);
    GopFrame primary_ref_frame =
        ref_manager.GetRefFrameByIndex(ref_frame.index);
    // The GetPrimaryRefFrame should find the ref_frame with matched layer depth
    // because it's our first priority
    EXPECT_EQ(primary_ref_frame.layer_depth, gop_frame.layer_depth);
  }

  const std::vector<int> mid_layer_depth_list = { 1, 3, 5 };
  for (int i = 0; i < 3; ++i) {
    // Test frame that share the same frame type with a reference frame
    GopFrameType type = type_list[i];
    // Let the frame layer_depth sit in the middle of two reference frames
    int layer_depth = mid_layer_depth_list[i];
    GopFrame gop_frame = GopFrameBasic(0, 0, 0, 0, layer_depth, type);
    ReferenceFrame ref_frame = ref_manager.GetPrimaryRefFrame(gop_frame);
    GopFrame primary_ref_frame =
        ref_manager.GetRefFrameByIndex(ref_frame.index);
    // The GetPrimaryRefFrame should find the ref_frame with matched frame type
    // Here we use coding_idx to confirm that.
    EXPECT_EQ(primary_ref_frame.coding_idx, i);
  }
}

TEST(RateControlQModeTest, TestKeyframeDetection) {
  FirstpassInfo firstpass_info;
  const std::string kFirstpassStatsFile = "firstpass_stats";
  ASSERT_NO_FATAL_FAILURE(
      ReadFirstpassInfo(kFirstpassStatsFile, &firstpass_info));
  EXPECT_THAT(GetKeyFrameList(firstpass_info),
              ElementsAre(0, 30, 60, 90, 120, 150, 180, 210, 240));
}

MATCHER_P(GopFrameMatches, expected, "") {
#define COMPARE_FIELD(FIELD)                                   \
  do {                                                         \
    if (arg.FIELD != expected.FIELD) {                         \
      *result_listener << "where " #FIELD " is " << arg.FIELD  \
                       << " but should be " << expected.FIELD; \
      return false;                                            \
    }                                                          \
  } while (0)
  COMPARE_FIELD(is_valid);
  COMPARE_FIELD(order_idx);
  COMPARE_FIELD(coding_idx);
  COMPARE_FIELD(global_order_idx);
  COMPARE_FIELD(global_coding_idx);
  COMPARE_FIELD(is_key_frame);
  COMPARE_FIELD(is_arf_frame);
  COMPARE_FIELD(is_show_frame);
  COMPARE_FIELD(is_golden_frame);
  COMPARE_FIELD(colocated_ref_idx);
  COMPARE_FIELD(update_ref_idx);
  COMPARE_FIELD(layer_depth);
#undef COMPARE_FIELD

  return true;
}

// Helper for tests which need to set update_ref_idx, but for which the indices
// and depth don't matter (other than to allow creating multiple GopFrames which
// are distinguishable).
GopFrame GopFrameUpdateRefIdx(int index, GopFrameType gop_frame_type,
                              int update_ref_idx) {
  GopFrame frame =
      GopFrameBasic(index, index, index, index, /*depth=*/0, gop_frame_type);
  frame.update_ref_idx = update_ref_idx;
  return frame;
}

TEST(RateControlQModeTest, TestGetRefFrameTableListFirstGop) {
  AV1RateControlQMode rc;
  RateControlParam rc_param;
  rc_param.ref_frame_table_size = 3;
  rc.SetRcParam(rc_param);

  const auto invalid = GopFrameInvalid();
  const auto frame0 = GopFrameUpdateRefIdx(0, GopFrameType::kRegularKey, -1);
  const auto frame1 = GopFrameUpdateRefIdx(1, GopFrameType::kRegularLeaf, 2);
  const auto frame2 = GopFrameUpdateRefIdx(2, GopFrameType::kRegularLeaf, 0);

  const auto matches_invalid = GopFrameMatches(invalid);
  const auto matches_frame0 = GopFrameMatches(frame0);
  const auto matches_frame1 = GopFrameMatches(frame1);
  const auto matches_frame2 = GopFrameMatches(frame2);

  GopStruct gop_struct;
  gop_struct.global_coding_idx_offset = 0;  // This is the first GOP.
  gop_struct.gop_frame_list = { frame0, frame1, frame2 };
  ASSERT_THAT(
      // For the first GOP only, GetRefFrameTableList can be passed a
      // default-constructed RefFrameTable (because it's all going to be
      // replaced by the key frame anyway).
      rc.GetRefFrameTableList(gop_struct, RefFrameTable()),
      ElementsAre(
          ElementsAre(matches_invalid, matches_invalid, matches_invalid),
          ElementsAre(matches_frame0, matches_frame0, matches_frame0),
          ElementsAre(matches_frame0, matches_frame0, matches_frame1),
          ElementsAre(matches_frame2, matches_frame0, matches_frame1)));
}

TEST(RateControlQModeTest, TestGetRefFrameTableListNotFirstGop) {
  AV1RateControlQMode rc;
  RateControlParam rc_param;
  rc_param.ref_frame_table_size = 3;
  rc.SetRcParam(rc_param);

  const auto previous = GopFrameUpdateRefIdx(0, GopFrameType::kRegularKey, -1);
  const auto frame0 = GopFrameUpdateRefIdx(5, GopFrameType::kRegularLeaf, 2);
  const auto frame1 = GopFrameUpdateRefIdx(6, GopFrameType::kRegularLeaf, -1);
  const auto frame2 = GopFrameUpdateRefIdx(7, GopFrameType::kRegularLeaf, 0);

  const auto matches_previous = GopFrameMatches(previous);
  const auto matches_frame0 = GopFrameMatches(frame0);
  const auto matches_frame2 = GopFrameMatches(frame2);

  GopStruct gop_struct;
  gop_struct.global_coding_idx_offset = 5;  // This is not the first GOP.
  gop_struct.gop_frame_list = { frame0, frame1, frame2 };
  ASSERT_THAT(
      rc.GetRefFrameTableList(gop_struct, RefFrameTable(3, previous)),
      ElementsAre(
          ElementsAre(matches_previous, matches_previous, matches_previous),
          ElementsAre(matches_previous, matches_previous, matches_frame0),
          ElementsAre(matches_previous, matches_previous, matches_frame0),
          ElementsAre(matches_frame2, matches_previous, matches_frame0)));
}

TEST(RateControlQModeTest, TestGopIntervals) {
  FirstpassInfo firstpass_info;
  ASSERT_NO_FATAL_FAILURE(
      ReadFirstpassInfo("firstpass_stats", &firstpass_info));
  AV1RateControlQMode rc;
  RateControlParam rc_param;
  rc_param.frame_height = 288;
  rc_param.frame_width = 352;
  rc_param.max_gop_show_frame_count = 32;
  rc_param.min_gop_show_frame_count = 4;
  rc_param.ref_frame_table_size = 7;
  rc.SetRcParam(rc_param);
  GopStructList gop_list = rc.DetermineGopInfo(firstpass_info);
  std::vector<int> gop_interval_list;
  std::transform(gop_list.begin(), gop_list.end(),
                 std::back_inserter(gop_interval_list),
                 [](GopStruct const &x) { return x.show_frame_count; });
  EXPECT_THAT(gop_interval_list,
              ElementsAre(21, 9, 30, 30, 30, 21, 9, 30, 12, 16, 2, 30));
}

// MockRateControlQMode is provided for the use of clients of libaom, but it's
// not expected that it will be used in any real libaom tests.
// This simple "toy" test exists solely to verify the integration of gmock into
// the aom build.
TEST(RateControlQModeTest, TestMock) {
  MockRateControlQMode mock_rc;
  EXPECT_CALL(mock_rc,
              DetermineGopInfo(Field(&FirstpassInfo::num_mbs_16x16, 1000)))
      .WillOnce(Return(GopStructList{ { 6, 0, 0, {} }, { 4, 0, 0, {} } }));
  FirstpassInfo firstpass_info = {};
  firstpass_info.num_mbs_16x16 = 1000;
  EXPECT_THAT(mock_rc.DetermineGopInfo(firstpass_info),
              ElementsAre(Field(&GopStruct::show_frame_count, 6),
                          Field(&GopStruct::show_frame_count, 4)));
}

}  // namespace aom

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  std::srand(0);
  return RUN_ALL_TESTS();
}
