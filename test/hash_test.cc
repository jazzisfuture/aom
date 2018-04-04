/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstdlib>
#include <new>

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "av1/encoder/hash.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

typedef uint32_t (*get_crc_value_func)(void *calculator, uint8_t *p,
                                       int length);

typedef ::testing::tuple<get_crc_value_func, int> HashParam;

class AV1CrcHashTest : public ::testing::TestWithParam<HashParam> {
 public:
  ~AV1CrcHashTest();
  void SetUp();

  void TearDown();

 protected:
  void RunCheckOutput(get_crc_value_func test_impl);
  void RunSpeedTest(get_crc_value_func test_impl);
  libaom_test::ACMRandom rnd_;
  CRC_CALCULATOR calc_;
  uint8_t *buffer_;
  int bsize_;
  int length_;
};

AV1CrcHashTest::~AV1CrcHashTest() { ; }

void AV1CrcHashTest::SetUp() {
  rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  av1_crc_calculator_init(&calc_);
  bsize_ = GET_PARAM(1);
  length_ = bsize_ * bsize_ * sizeof(uint16_t);
  buffer_ = new uint8_t[length_];
  ASSERT_TRUE(buffer_ != NULL);
  for (int i = 0; i < length_; ++i) {
    buffer_[i] = rnd_.Rand8();
  }
}

void AV1CrcHashTest::TearDown() { delete[] buffer_; }

void AV1CrcHashTest::RunCheckOutput(get_crc_value_func test_impl) {
  get_crc_value_func ref_impl = av1_get_crc_value_c;
  // for the same buffer crc should be the same
  uint32_t crc0 = test_impl(&calc_, buffer_, length_);
  uint32_t crc1 = test_impl(&calc_, buffer_, length_);
  uint32_t crc2 = ref_impl(&calc_, buffer_, length_);
  ASSERT_EQ(crc0, crc1);
  ASSERT_EQ(crc0, crc2);  // should equal to software version
  // modify buffer
  buffer_[0] += 1;
  uint32_t crc3 = test_impl(&calc_, buffer_, length_);
  uint32_t crc4 = ref_impl(&calc_, buffer_, length_);
  ASSERT_NE(crc0, crc3);  // crc shoud not equal to previous one
  ASSERT_EQ(crc3, crc4);

  int16_t buf1[16] = {
    8, 11, 12, 4, 6, 6, 8, 3, -5, -7, -8, -8, -4, -6, -7, -7,
  };
  int16_t buf2[16] = {
    4, 2, -1, 2, 5, 3, 4, 6, 6, 4, 10, 6, 16, 16, 14, -15,
  };
  uint32_t crc5 = (test_impl(&calc_, (uint8_t *)buf1, 32) << 5);
  uint32_t crc6 = (test_impl(&calc_, (uint8_t *)buf2, 32) << 5);
  ASSERT_NE(crc5, crc6);
}

void AV1CrcHashTest::RunSpeedTest(get_crc_value_func test_impl) {
  get_crc_value_func impls[] = { av1_get_crc_value_c, test_impl };
  const int repeat = 10000000 / (bsize_ + bsize_);

  aom_usec_timer timer;
  double time[2];
  for (int i = 0; i < 2; ++i) {
    aom_usec_timer_start(&timer);
    for (int j = 0; j < repeat; ++j) {
      impls[i](&calc_, buffer_, length_);
    }
    aom_usec_timer_mark(&timer);
    time[i] = static_cast<double>(aom_usec_timer_elapsed(&timer));
  }
  printf("hash %3dx%-3d:%7.2f/%7.2fus", bsize_, bsize_, time[0], time[1]);
  printf("(%3.2f)\n", time[0] / time[1]);
}

TEST_P(AV1CrcHashTest, CheckOutput) { RunCheckOutput(GET_PARAM(0)); }

TEST_P(AV1CrcHashTest, DISABLED_Speed) { RunSpeedTest(GET_PARAM(0)); }

const int kValidBlockSize[] = { 64, 32, 8, 4 };

INSTANTIATE_TEST_CASE_P(
    C, AV1CrcHashTest,
    ::testing::Combine(::testing::Values(&av1_get_crc_value_c),
                       ::testing::ValuesIn(kValidBlockSize)));

#if HAVE_SSE4_2
INSTANTIATE_TEST_CASE_P(
    SSE4_2, AV1CrcHashTest,
    ::testing::Combine(::testing::Values(&av1_get_crc_value_sse4_2),
                       ::testing::ValuesIn(kValidBlockSize)));
#endif

}  // namespace
