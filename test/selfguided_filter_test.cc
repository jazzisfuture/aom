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

#include <ctime>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "aom_ports/aom_timer.h"
#include "av1/common/mv.h"
#include "av1/common/restoration.h"

namespace {

using std::tr1::tuple;
using std::tr1::make_tuple;
using libaom_test::ACMRandom;

typedef void (*SgrFunc)(const uint8_t *dat8, int width, int height, int stride,
                        int eps, const int *xqd, uint8_t *dst8, int dst_stride,
                        int32_t *tmpbuf, int bit_depth, int highbd);

// Test parameter list:
//  <tst_fun_>
typedef tuple<SgrFunc> FilterTestParam;

class AV1SelfguidedFilterTest
    : public ::testing::TestWithParam<FilterTestParam> {
 public:
  virtual ~AV1SelfguidedFilterTest() {}
  virtual void SetUp() {}

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunSpeedTest() {
    tst_fun_ = GET_PARAM(0);
    const int pu_width = RESTORATION_PROC_UNIT_SIZE;
    const int pu_height = RESTORATION_PROC_UNIT_SIZE;
    const int width = 256, height = 256, stride = 288, out_stride = 288;
    const int NUM_ITERS = 2000;
    int i, j, k;

    uint8_t *input_ =
        (uint8_t *)aom_memalign(32, stride * (height + 32) * sizeof(uint8_t));
    uint8_t *output_ = (uint8_t *)aom_memalign(
        32, out_stride * (height + 32) * sizeof(uint8_t));
    int32_t *tmpbuf = (int32_t *)aom_memalign(32, RESTORATION_TMPBUF_SIZE);
    uint8_t *input = input_ + stride * 16 + 16;
    uint8_t *output = output_ + out_stride * 16 + 16;

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    for (i = -16; i < height + 16; ++i)
      for (j = -16; j < width + 16; ++j)
        input[i * stride + j] = rnd.Rand16() & 0xFF;

    int xqd[2] = {
      SGRPROJ_PRJ_MIN0 +
          rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 - SGRPROJ_PRJ_MIN0),
      SGRPROJ_PRJ_MIN1 +
          rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 - SGRPROJ_PRJ_MIN1)
    };
    // Fix a parameter set, since the speed depends slightly on r.
    // Change this to test different combinations of values of r.
    int eps = 15;

    av1_loop_restoration_precal();

    std::clock_t start = std::clock();
    for (i = 0; i < NUM_ITERS; ++i) {
      for (k = 0; k < height; k += pu_height)
        for (j = 0; j < width; j += pu_width) {
          int w = AOMMIN(pu_width, width - j);
          int h = AOMMIN(pu_height, height - k);
          uint8_t *input_p = input + k * stride + j;
          uint8_t *output_p = output + k * out_stride + j;
          tst_fun_(input_p, w, h, stride, eps, xqd, output_p, out_stride,
                   tmpbuf, 8, 0);
        }
    }
    std::clock_t end = std::clock();
    double elapsed = ((end - start) / (double)CLOCKS_PER_SEC);

    printf("%5d %dx%d blocks in %7.3fs = %7.3fus/block\n", NUM_ITERS, width,
           height, elapsed, elapsed * 1000000. / NUM_ITERS);

    aom_free(input_);
    aom_free(output_);
    aom_free(tmpbuf);
  }

  void RunCorrectnessTest() {
    tst_fun_ = GET_PARAM(0);
    const int pu_width = RESTORATION_PROC_UNIT_SIZE;
    const int pu_height = RESTORATION_PROC_UNIT_SIZE;
    // Set the maximum width/height to test here. We actually test a small
    // range of sizes *up to* this size, so that we can check, eg.,
    // the behaviour on tiles which are not a multiple of 4 wide.
    const int max_w = 260, max_h = 260, stride = 672, out_stride = 672;
    const int NUM_ITERS = 81;
    int i, j, k;

    uint8_t *input_ =
        (uint8_t *)aom_memalign(32, stride * (max_h + 32) * sizeof(uint8_t));
    uint8_t *output_ = (uint8_t *)aom_memalign(
        32, out_stride * (max_h + 32) * sizeof(uint8_t));
    uint8_t *output2_ = (uint8_t *)aom_memalign(
        32, out_stride * (max_h + 32) * sizeof(uint8_t));
    int32_t *tmpbuf = (int32_t *)aom_memalign(32, RESTORATION_TMPBUF_SIZE);

    uint8_t *input = input_ + stride * 16 + 16;
    uint8_t *output = output_ + out_stride * 16 + 16;
    uint8_t *output2 = output2_ + out_stride * 16 + 16;

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    av1_loop_restoration_precal();

    for (i = 0; i < NUM_ITERS; ++i) {
      for (j = -16; j < max_h + 16; ++j)
        for (k = -16; k < max_w + 16; ++k)
          input[j * stride + k] = rnd.Rand16() & 0xFF;

      int xqd[2] = {
        SGRPROJ_PRJ_MIN0 +
            rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 - SGRPROJ_PRJ_MIN0),
        SGRPROJ_PRJ_MIN1 +
            rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 - SGRPROJ_PRJ_MIN1)
      };
      int eps = rnd.PseudoUniform(1 << SGRPROJ_PARAMS_BITS);

      // Test various tile sizes around 256x256
      int test_w = max_w - (i / 9);
      int test_h = max_h - (i % 9);

      for (k = 0; k < test_h; k += pu_height)
        for (j = 0; j < test_w; j += pu_width) {
          int w = AOMMIN(pu_width, test_w - j);
          int h = AOMMIN(pu_height, test_h - k);
          uint8_t *input_p = input + k * stride + j;
          uint8_t *output_p = output + k * out_stride + j;
          uint8_t *output2_p = output2 + k * out_stride + j;
          tst_fun_(input_p, w, h, stride, eps, xqd, output_p, out_stride,
                   tmpbuf, 8, 0);
          apply_selfguided_restoration_c(input_p, w, h, stride, eps, xqd,
                                         output2_p, out_stride, tmpbuf, 8, 0);
        }

      for (j = 0; j < test_h; ++j)
        for (k = 0; k < test_w; ++k) {
          ASSERT_EQ(output[j * out_stride + k], output2[j * out_stride + k]);
        }
    }

    aom_free(input_);
    aom_free(output_);
    aom_free(output2_);
    aom_free(tmpbuf);
  }

 private:
  SgrFunc tst_fun_;
};

TEST_P(AV1SelfguidedFilterTest, DISABLED_SpeedTest) { RunSpeedTest(); }
TEST_P(AV1SelfguidedFilterTest, CorrectnessTest) { RunCorrectnessTest(); }

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(SSE4_1, AV1SelfguidedFilterTest,
                        ::testing::Values(apply_selfguided_restoration_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_CASE_P(AVX2, AV1SelfguidedFilterTest,
                        ::testing::Values(apply_selfguided_restoration_avx2));
#endif

// Test parameter list:
//  <tst_fun_, bit_depth>
typedef tuple<SgrFunc, int> HighbdFilterTestParam;

class AV1HighbdSelfguidedFilterTest
    : public ::testing::TestWithParam<HighbdFilterTestParam> {
 public:
  virtual ~AV1HighbdSelfguidedFilterTest() {}
  virtual void SetUp() {}

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunSpeedTest() {
    tst_fun_ = GET_PARAM(0);
    const int pu_width = RESTORATION_PROC_UNIT_SIZE;
    const int pu_height = RESTORATION_PROC_UNIT_SIZE;
    const int width = 256, height = 256, stride = 288, out_stride = 288;
    const int NUM_ITERS = 2000;
    int i, j, k;
    int bit_depth = GET_PARAM(1);
    int mask = (1 << bit_depth) - 1;

    uint16_t *input_ =
        (uint16_t *)aom_memalign(32, stride * (height + 32) * sizeof(uint16_t));
    uint16_t *output_ = (uint16_t *)aom_memalign(
        32, out_stride * (height + 32) * sizeof(uint16_t));
    int32_t *tmpbuf = (int32_t *)aom_memalign(32, RESTORATION_TMPBUF_SIZE);
    uint16_t *input = input_ + stride * 16 + 16;
    uint16_t *output = output_ + out_stride * 16 + 16;

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    for (i = -16; i < height + 16; ++i)
      for (j = -16; j < width + 16; ++j)
        input[i * stride + j] = rnd.Rand16() & mask;

    int xqd[2] = {
      SGRPROJ_PRJ_MIN0 +
          rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 - SGRPROJ_PRJ_MIN0),
      SGRPROJ_PRJ_MIN1 +
          rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 - SGRPROJ_PRJ_MIN1)
    };
    // Fix a parameter set, since the speed depends slightly on r.
    // Change this to test different combinations of values of r.
    int eps = 15;

    av1_loop_restoration_precal();

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (i = 0; i < NUM_ITERS; ++i) {
      for (k = 0; k < height; k += pu_height)
        for (j = 0; j < width; j += pu_width) {
          int w = AOMMIN(pu_width, width - j);
          int h = AOMMIN(pu_height, height - k);
          uint16_t *input_p = input + k * stride + j;
          uint16_t *output_p = output + k * out_stride + j;
          tst_fun_(CONVERT_TO_BYTEPTR(input_p), w, h, stride, eps, xqd,
                   CONVERT_TO_BYTEPTR(output_p), out_stride, tmpbuf, bit_depth,
                   1);
        }
    }
    aom_usec_timer_mark(&timer);
    double elapsed = static_cast<double>(aom_usec_timer_elapsed(&timer));

    printf("%5d %dx%d blocks in %7.3fs = %7.3fus/block\n", NUM_ITERS, width,
           height, elapsed / 1000000, elapsed / NUM_ITERS);

    aom_free(input_);
    aom_free(output_);
    aom_free(tmpbuf);
  }

  void RunCorrectnessTest() {
    tst_fun_ = GET_PARAM(0);
    const int pu_width = RESTORATION_PROC_UNIT_SIZE;
    const int pu_height = RESTORATION_PROC_UNIT_SIZE;
    // Set the maximum width/height to test here. We actually test a small
    // range of sizes *up to* this size, so that we can check, eg.,
    // the behaviour on tiles which are not a multiple of 4 wide.
    const int max_w = 260, max_h = 260, stride = 672, out_stride = 672;
    const int NUM_ITERS = 81;
    int i, j, k;
    int bit_depth = GET_PARAM(1);
    int mask = (1 << bit_depth) - 1;

    uint16_t *input_ =
        (uint16_t *)aom_memalign(32, stride * (max_h + 32) * sizeof(uint16_t));
    uint16_t *output_ = (uint16_t *)aom_memalign(
        32, out_stride * (max_h + 32) * sizeof(uint16_t));
    uint16_t *output2_ = (uint16_t *)aom_memalign(
        32, out_stride * (max_h + 32) * sizeof(uint16_t));
    int32_t *tmpbuf = (int32_t *)aom_memalign(32, RESTORATION_TMPBUF_SIZE);

    uint16_t *input = input_ + stride * 16 + 16;
    uint16_t *output = output_ + out_stride * 16 + 16;
    uint16_t *output2 = output2_ + out_stride * 16 + 16;

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    av1_loop_restoration_precal();

    for (i = 0; i < NUM_ITERS; ++i) {
      for (j = -16; j < max_h + 16; ++j)
        for (k = -16; k < max_w + 16; ++k)
          input[j * stride + k] = rnd.Rand16() & mask;

      int xqd[2] = {
        SGRPROJ_PRJ_MIN0 +
            rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 - SGRPROJ_PRJ_MIN0),
        SGRPROJ_PRJ_MIN1 +
            rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 - SGRPROJ_PRJ_MIN1)
      };
      int eps = rnd.PseudoUniform(1 << SGRPROJ_PARAMS_BITS);

      // Test various tile sizes around 256x256
      int test_w = max_w - (i / 9);
      int test_h = max_h - (i % 9);

      for (k = 0; k < test_h; k += pu_height)
        for (j = 0; j < test_w; j += pu_width) {
          int w = AOMMIN(pu_width, test_w - j);
          int h = AOMMIN(pu_height, test_h - k);
          uint16_t *input_p = input + k * stride + j;
          uint16_t *output_p = output + k * out_stride + j;
          uint16_t *output2_p = output2 + k * out_stride + j;
          tst_fun_(CONVERT_TO_BYTEPTR(input_p), w, h, stride, eps, xqd,
                   CONVERT_TO_BYTEPTR(output_p), out_stride, tmpbuf, bit_depth,
                   1);
          apply_selfguided_restoration_c(
              CONVERT_TO_BYTEPTR(input_p), w, h, stride, eps, xqd,
              CONVERT_TO_BYTEPTR(output2_p), out_stride, tmpbuf, bit_depth, 1);
        }

      for (j = 0; j < test_h; ++j)
        for (k = 0; k < test_w; ++k)
          ASSERT_EQ(output[j * out_stride + k], output2[j * out_stride + k]);
    }

    aom_free(input_);
    aom_free(output_);
    aom_free(output2_);
    aom_free(tmpbuf);
  }

 private:
  SgrFunc tst_fun_;
};

TEST_P(AV1HighbdSelfguidedFilterTest, DISABLED_SpeedTest) { RunSpeedTest(); }
TEST_P(AV1HighbdSelfguidedFilterTest, CorrectnessTest) { RunCorrectnessTest(); }

#if HAVE_SSE4_1
const int highbd_params_sse4_1[] = { 8, 10, 12 };
INSTANTIATE_TEST_CASE_P(
    SSE4_1, AV1HighbdSelfguidedFilterTest,
    ::testing::Combine(::testing::Values(apply_selfguided_restoration_sse4_1),
                       ::testing::ValuesIn(highbd_params_sse4_1)));
#endif

#if HAVE_AVX2
const int highbd_params_avx2[] = { 8, 10, 12 };
INSTANTIATE_TEST_CASE_P(
    AVX2, AV1HighbdSelfguidedFilterTest,
    ::testing::Combine(::testing::Values(apply_selfguided_restoration_avx2),
                       ::testing::ValuesIn(highbd_params_avx2)));
#endif

}  // namespace
