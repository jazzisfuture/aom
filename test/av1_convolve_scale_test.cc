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

#include <vector>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {
const int PERF_ITERS = 1000;

const int kVPad = 32;
const int kHPad = 32;
const int x_step_qn = 16;
const int y_step_qn = 20;

using std::tr1::tuple;
using std::tr1::make_tuple;
using libaom_test::ACMRandom;

enum NTaps { EIGHT_TAP, TEN_TAP, TWELVE_TAP };
int NTapsToInt(NTaps ntaps) { return 8 + static_cast<int>(ntaps) * 2; }

// A 16-bit filter with a configurable number of taps.
class TestFilter {
 public:
  void set(NTaps ntaps, bool backwards);

  InterpFilterParams params_;

 private:
  std::vector<int16_t> coeffs_;
};

void TestFilter::set(NTaps ntaps, bool backwards) {
  const int n = NTapsToInt(ntaps);
  assert(n >= 8 && n <= 12);

  // The filter has n * SUBPEL_SHIFTS proper elements and an extra 8 bogus
  // elements at the end so that convolutions can read off the end safely.
  coeffs_.resize(n * SUBPEL_SHIFTS + 8);

  // The coefficients are pretty much arbitrary, but convolutions shouldn't
  // over or underflow. For the first filter (subpels = 0), we use an
  // increasing or decreasing ramp (depending on the backwards parameter). We
  // don't want any zero coefficients, so we make it have an x-intercept at -1
  // or n. To ensure absence of under/overflow, we normalise the area under the
  // ramp to be I = 1 << FILTER_BITS (so that convolving a constant function
  // gives the identity).
  //
  // When increasing, the function has the form:
  //
  //   f(x) = A * (x + 1)
  //
  // Summing and rearranging for A gives A = 2 * I / (n * (n + 1)). If the
  // filter is reversed, we have the same A but with formula
  //
  //   g(x) = A * (n - x)
  const int I = 1 << FILTER_BITS;
  const float A = 2.f * I / (n * (n + 1.f));
  for (int i = 0; i < n; ++i) {
    coeffs_[i] = static_cast<int16_t>(A * (backwards ? (n - i) : (i + 1)));
  }

  // For the other filters, make them slightly different by swapping two
  // columns. Filter k will have the columns (k % n) and (7 * k) % n swapped.
  const size_t filter_size = sizeof(coeffs_[0] * n);
  int16_t *const filter0 = &coeffs_[0];
  for (int k = 1; k < SUBPEL_SHIFTS; ++k) {
    int16_t *filterk = &coeffs_[k * n];
    memcpy(filterk, filter0, filter_size);

    const int idx0 = k % n;
    const int idx1 = (7 * k) % n;

    const int16_t tmp = filterk[idx0];
    filterk[idx0] = filterk[idx1];
    filterk[idx1] = tmp;
  }

  // Finally, write some rubbish at the end to make sure we don't use it.
  for (int i = 0; i < 8; ++i) coeffs_[n * SUBPEL_SHIFTS + i] = 123 + i;

  // Fill in params
  params_.filter_ptr = &coeffs_[0];
  params_.taps = n;
  // These are ignored by the functions being tested. Set them to whatever.
  params_.subpel_shifts = SUBPEL_SHIFTS;
  params_.interp_filter = EIGHTTAP_REGULAR;
}

template <typename SrcPixel>
class TestImage {
 public:
  TestImage(int w, int h, int bd) : w_(w), h_(h), bd_(bd) {
    assert(bd < 16);
    assert(bd <= 8 * (int)sizeof(SrcPixel));

    // Pad width by 2*kHPad and then round up to the next multiple of 16
    // to get src_stride_. Add another 16 for dst_stride_ (to make sure
    // something goes wrong if we use the wrong one)
    src_stride_ = (w_ + 2 * kHPad + 15) & ~15;
    dst_stride_ = src_stride_ + 16;

    // Allocate image data
    src_data_.resize(src_block_size());
    dst_data_.resize(dst_block_size());
  }

  void Initialize(ACMRandom *rnd);

  int src_stride() const { return src_stride_; }
  int dst_stride() const { return dst_stride_; }

  int src_block_size() const { return (h_ + 2 * kVPad) * src_stride(); }
  int dst_block_size() const { return (h_ + 2 * kVPad) * dst_stride(); }

  const SrcPixel *GetSrcData(bool borders) const {
    const SrcPixel *block = &src_data_[0];
    return borders ? block : block + kHPad + src_stride_ * kVPad;
  }

  int32_t *GetDstData(bool borders) {
    int32_t *block = &dst_data_[0];
    return borders ? block : block + kHPad + dst_stride_ * kVPad;
  }

 private:
  int w_, h_, bd_;
  int src_stride_, dst_stride_;

  std::vector<SrcPixel> src_data_;
  std::vector<int32_t> dst_data_;
};

template <typename pixel_t>
void FillEdge(ACMRandom *rnd, int nelts, int bd, bool trash, pixel_t *data) {
  if (!trash) {
    memset(data, 0, sizeof(*data) * nelts);
    return;
  }
  pixel_t mask = (1 << bd) - 1;
  for (int i = 0; i < nelts; ++i) data[i] = rnd->Rand16() & mask;
}

template <typename pixel_t>
void PrepBuffers(ACMRandom *rnd, int w, int h, int stride, int bd,
                 bool trash_edges, pixel_t *data) {
  assert(rnd);
  pixel_t mask = (1 << bd) - 1;

  // Fill in the image with random data
  // Top border
  FillEdge(rnd, stride * kVPad, bd, trash_edges, data);
  for (int r = 0; r < h; ++r) {
    pixel_t *row_data = data + (kVPad + r) * stride;
    // Left border, contents, right border
    FillEdge(rnd, kHPad, bd, trash_edges, row_data);
    for (int c = 0; c < w; ++c) row_data[kHPad + c] = rnd->Rand16() & mask;
    FillEdge(rnd, kHPad, bd, trash_edges, row_data + kHPad + w);
  }
  // Bottom border
  FillEdge(rnd, stride * kVPad, bd, trash_edges, data + stride * (kVPad + h));
}

template <typename SrcPixel>
void TestImage<SrcPixel>::Initialize(ACMRandom *rnd) {
  PrepBuffers(rnd, w_, h_, src_stride_, bd_, false, &src_data_[0]);
  PrepBuffers(rnd, w_, h_, dst_stride_, bd_, true, &dst_data_[0]);
}

typedef tuple<int, int> BlockDimension;

struct BaseParams {
  BaseParams(BlockDimension dims, NTaps ntaps_x, NTaps ntaps_y, bool avg)
      : dims(dims), ntaps_x(ntaps_x), ntaps_y(ntaps_y), avg(avg) {}

  BlockDimension dims;
  NTaps ntaps_x, ntaps_y;
  bool avg;
};

template <typename SrcPixel>
class ConvolveScaleTestBase : public ::testing::Test {
 public:
  ConvolveScaleTestBase() : image_(NULL) {}
  virtual ~ConvolveScaleTestBase() { delete image_; }
  virtual void TearDown() { libaom_test::ClearSystemState(); }

  // Implemented by subclasses (SetUp depends on the parameters passed
  // in and RunOne depends on the function to be tested. These can't
  // be templated for low/high bit depths because they have different
  // numbers of parameters)
  virtual void SetUp() = 0;
  virtual void RunOne() = 0;

 protected:
  void SetParams(const BaseParams &params, int bd) {
    width_ = std::tr1::get<0>(params.dims);
    height_ = std::tr1::get<1>(params.dims);
    ntaps_x_ = params.ntaps_x;
    ntaps_y_ = params.ntaps_y;
    bd_ = bd;
    avg_ = params.avg;

    filter_x_.set(ntaps_x_, false);
    filter_y_.set(ntaps_y_, true);
    convolve_params_ = get_conv_params_no_round(0, avg_ != false, 0, NULL, 0);

    delete image_;
    image_ = new TestImage<SrcPixel>(width_, height_, bd_);
  }

  void SpeedTest() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    Prep(&rnd);

    aom_usec_timer ref_timer;
    aom_usec_timer_start(&ref_timer);
    for (int i = 0; i < PERF_ITERS; ++i) RunOne();
    aom_usec_timer_mark(&ref_timer);
    int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

    std::cout << "[          ] C time = " << ref_time / 1000 << " ms\n";
  }

  static int RandomSubpel(ACMRandom *rnd) {
    uint8_t subpel_mode = rnd->Rand8();
    if ((subpel_mode & 7) == 0) {
      return 0;
    } else if ((subpel_mode & 7) == 1) {
      return SCALE_SUBPEL_SHIFTS - 1;
    } else {
      return 1 + rnd->PseudoUniform(SCALE_SUBPEL_SHIFTS - 2);
    }
  }

  void Prep(ACMRandom *rnd) {
    assert(rnd);

    // Choose subpel_x_ and subpel_y_. They should be less than
    // SCALE_SUBPEL_SHIFTS; we also want to add extra weight to "interesting"
    // values: 0 and SCALE_SUBPEL_SHIFTS - 1
    subpel_x_ = RandomSubpel(rnd);
    subpel_y_ = RandomSubpel(rnd);

    image_->Initialize(rnd);
  }

  int width_, height_, bd_;
  NTaps ntaps_x_, ntaps_y_;
  bool avg_;
  int subpel_x_, subpel_y_;
  TestFilter filter_x_, filter_y_;
  TestImage<SrcPixel> *image_;
  ConvolveParams convolve_params_;
};

typedef tuple<int, int> BlockDimension;

// Test parameter list:
//  <tst_fun, dims, ntaps_x, ntaps_y, avg>
typedef tuple<BlockDimension, NTaps, NTaps, bool> LowBDParams;

class LowBDConvolveScaleTest
    : public ConvolveScaleTestBase<uint8_t>,
      public ::testing::WithParamInterface<LowBDParams> {
 public:
  virtual ~LowBDConvolveScaleTest() {}

  void SetUp() {
    const BlockDimension &block = GET_PARAM(0);
    NTaps ntaps_x = GET_PARAM(1);
    NTaps ntaps_y = GET_PARAM(2);
    int bd = 8;
    bool avg = GET_PARAM(3);

    SetParams(BaseParams(block, ntaps_x, ntaps_y, avg), bd);
  }

  void RunOne() {
    const uint8_t *src = image_->GetSrcData(false);
    CONV_BUF_TYPE *dst = image_->GetDstData(false);
    int src_stride = image_->src_stride();
    int dst_stride = image_->dst_stride();

    av1_convolve_2d_scale_c(src, src_stride, dst, dst_stride, width_, height_,
                            &filter_x_.params_, &filter_y_.params_, subpel_x_,
                            x_step_qn, subpel_y_, y_step_qn, &convolve_params_);
  }
};

const BlockDimension kBlockDim[] = {
  make_tuple(2, 2),    make_tuple(2, 4),    make_tuple(4, 4),
  make_tuple(4, 8),    make_tuple(8, 4),    make_tuple(8, 8),
  make_tuple(8, 16),   make_tuple(16, 8),   make_tuple(16, 16),
  make_tuple(16, 32),  make_tuple(32, 16),  make_tuple(32, 32),
  make_tuple(32, 64),  make_tuple(64, 32),  make_tuple(64, 64),
  make_tuple(64, 128), make_tuple(128, 64), make_tuple(128, 128),
};

const NTaps kNTaps[] = { EIGHT_TAP, TEN_TAP, TWELVE_TAP };

TEST_P(LowBDConvolveScaleTest, DISABLED_Speed) { SpeedTest(); }

INSTANTIATE_TEST_CASE_P(SSE4_1, LowBDConvolveScaleTest,
                        ::testing::Combine(::testing::ValuesIn(kBlockDim),
                                           ::testing::ValuesIn(kNTaps),
                                           ::testing::ValuesIn(kNTaps),
                                           ::testing::Bool()));
}  // namespace
