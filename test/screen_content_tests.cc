/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <ostream>

#include "aom/aom_codec.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/y4m_video_source.h"
#include "test/util.h"

namespace {
// This class is used to validate if screen_content_tools are turnd on
// appropriately.
class ScreenContentToolsTestLarge
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode,
                                                 aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  ScreenContentToolsTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        end_usage_check_(GET_PARAM(2)) {
    allow_screen_content_tools_ = false;
    allow_intra_bc_ = false;
    tune_content_ = AOM_CONTENT_DEFAULT;
  }
  virtual ~ScreenContentToolsTestLarge() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = end_usage_check_;
    cfg_.g_threads = 1;
    cfg_.g_lag_in_frames = 19;
  }

  virtual bool DoDecode() const { return 1; }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AV1E_SET_TUNE_CONTENT, tune_content_);
    }
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      int sc_info[3] = {
        0,  // allow_screen_content_tools
        0,  // allow_intra_bc
        0,  // force_integer_mv
      };

      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_SCREEN_CONTENT_TOOLS_INFO,
                                    sc_info);
      if (sc_info[0] == 1) {
        allow_screen_content_tools_ = true;
      }
      if (sc_info[1] == 1) {
        allow_intra_bc_ = true;
      }
    }
    return AOM_CODEC_OK == res_dec;
  }

  ::libaom_test::TestMode encoding_mode_;
  bool allow_screen_content_tools_;
  bool allow_intra_bc_;
  int tune_content_;
  aom_rc_mode end_usage_check_;
};

TEST_P(ScreenContentToolsTestLarge, ScreenContentToolsTest) {
  ::libaom_test::Y4mVideoSource video("screendata.y4m", 0, 1);
  // force screen content tools on
  tune_content_ = AOM_CONTENT_SCREEN;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(allow_screen_content_tools_, true);
  ASSERT_EQ(allow_intra_bc_, true);

  // Don't force screen content, however as the input is screen content
  // allow_screen_content_tools should still be turned on
  tune_content_ = AOM_CONTENT_DEFAULT;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(allow_screen_content_tools_, true);
}

AV1_INSTANTIATE_TEST_CASE(ScreenContentToolsTestLarge,
                          ::testing::Values(::libaom_test::kOnePassGood,
                                            ::libaom_test::kTwoPassGood),
                          ::testing::Values(AOM_Q, AOM_VBR, AOM_CBR, AOM_CQ));
}  // namespace
