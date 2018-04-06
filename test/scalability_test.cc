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

#include "./aom_config.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

const int kCpuUsed = 8;
const int kBaseLayerQp = 55;
const int kEnhancementLayerQp = 20;

class ScalabilityTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  ScalabilityTest() : EncoderTest(GET_PARAM(0)) {}
  virtual ~ScalabilityTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(GET_PARAM(1));
    num_slayers_ = 2;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, kCpuUsed);
      encoder->Control(AOME_SET_NUMBER_SPATIAL_LAYERS, num_slayers_);
    } else if (video->frame() % num_slayers_) {
      frame_flags_ = AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
                     AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
                     AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
                     AOM_EFLAG_NO_UPD_LAST | AOM_EFLAG_NO_UPD_GF |
                     AOM_EFLAG_NO_UPD_ARF | AOM_EFLAG_NO_UPD_ENTROPY;
      encoder->Control(AOME_SET_ENHANCEMENT_LAYER_ID, 1);
      encoder->Control(AOME_SET_CQ_LEVEL, kEnhancementLayerQp);
    } else {
      frame_flags_ = AOM_EFLAG_NO_REF_LAST2 | AOM_EFLAG_NO_REF_LAST3 |
                     AOM_EFLAG_NO_REF_GF | AOM_EFLAG_NO_REF_ARF |
                     AOM_EFLAG_NO_REF_BWD | AOM_EFLAG_NO_REF_ARF2 |
                     AOM_EFLAG_NO_UPD_GF | AOM_EFLAG_NO_UPD_ARF |
                     AOM_EFLAG_NO_UPD_ENTROPY;
      encoder->Control(AOME_SET_ENHANCEMENT_LAYER_ID, 0);
      encoder->Control(AOME_SET_CQ_LEVEL, kBaseLayerQp);
    }
  }

  void DoTest(int num_slayers) {
    num_slayers_ = num_slayers;
    cfg_.rc_end_usage = AOM_Q;
    cfg_.g_lag_in_frames = 0;

    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 18);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int num_slayers_;
};

// Validate that this scalability mode (2 spatial layers)
// encodes and decodes without a mismatch.
TEST_P(ScalabilityTest, TestNoMisMatch2SLayers) { DoTest(2); }

// Validate that this scalability mode (3 spatial layers)
// encodes and decodes without a mismatch.
TEST_P(ScalabilityTest, TestNoMisMatch3SLayers) { DoTest(3); }

AV1_INSTANTIATE_TEST_CASE(ScalabilityTest,
                          ::testing::Values(::libaom_test::kRealTime));

}  // namespace
