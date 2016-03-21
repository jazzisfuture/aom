/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP10_DECODER_DSUBEXP_H_
#define VP10_DECODER_DSUBEXP_H_

#include "aom_dsp/bitreader.h"

#ifdef __cplusplus
extern "C" {
#endif

void vp10_diff_update_prob(vpx_reader *r, vpx_prob *p);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_DECODER_DSUBEXP_H_
