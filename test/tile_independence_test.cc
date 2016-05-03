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

#include <cstdio>
#include <cstdlib>
#include <string>
#include "aom_mem/aom_mem.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "third_party/googletest/src/include/gtest/gtest.h"

namespace {
class TileIndependenceTest : public ::libaom_test::EncoderTest,
                             public ::libaom_test::CodecTestWithParam<int> {
 protected:
  TileIndependenceTest()
      : EncoderTest(GET_PARAM(0)), md5_fw_order_(), md5_inv_order_(),
        n_tiles_(GET_PARAM(1)) {
    init_flags_ = AOM_CODEC_USE_PSNR;
    aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
    cfg.w = 704;
    cfg.h = 144;
    cfg.threads = 1;
    fw_dec_ = codec_->CreateDecoder(cfg, 0);
    inv_dec_ = codec_->CreateDecoder(cfg, 0);
    inv_dec_->Control(AV1_INVERT_TILE_DECODE_ORDER, 1);
  }

  virtual ~TileIndependenceTest() {
    delete fw_dec_;
    delete inv_dec_;
  }

  virtual void SetUp() {
    InitializeConfig();
    SetMode(libaom_test::kTwoPassGood);
  }

  virtual void PreEncodeFrameHook(libaom_test::VideoSource *video,
                                  libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(AV1E_SET_TILE_COLUMNS, n_tiles_);
    }
  }

  void UpdateMD5(::libaom_test::Decoder *dec, const aom_codec_cx_pkt_t *pkt,
                 ::libaom_test::MD5 *md5) {
    const aom_codec_err_t res = dec->DecodeFrame(
        reinterpret_cast<uint8_t *>(pkt->data.frame.buf), pkt->data.frame.sz);
    if (res != AOM_CODEC_OK) {
      abort_ = true;
      ASSERT_EQ(AOM_CODEC_OK, res);
    }
    const aom_image_t *img = dec->GetDxData().Next();
    md5->Add(img);
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    UpdateMD5(fw_dec_, pkt, &md5_fw_order_);
    UpdateMD5(inv_dec_, pkt, &md5_inv_order_);
  }

  ::libaom_test::MD5 md5_fw_order_, md5_inv_order_;
  ::libaom_test::Decoder *fw_dec_, *inv_dec_;

 private:
  int n_tiles_;
};

// run an encode with 2 or 4 tiles, and do the decode both in normal and
// inverted tile ordering. Ensure that the MD5 of the output in both cases
// is identical. If so, tiles are considered independent and the test passes.
TEST_P(TileIndependenceTest, MD5Match) {
  const aom_rational timebase = {33333333, 1000000000};
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = 500;
  cfg_.g_lag_in_frames = 25;
  cfg_.rc_end_usage = AOM_VBR;

  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 704, 144,
                                     timebase.den, timebase.num, 0, 30);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  const char *md5_fw_str = md5_fw_order_.Get();
  const char *md5_inv_str = md5_inv_order_.Get();

  // could use ASSERT_EQ(!memcmp(.., .., 16) here, but this gives nicer
  // output if it fails. Not sure if it's helpful since it's really just
  // a MD5...
  ASSERT_STREQ(md5_fw_str, md5_inv_str);
}

AV1_INSTANTIATE_TEST_CASE(TileIndependenceTest, ::testing::Range(0, 2, 1));

}  // namespace
