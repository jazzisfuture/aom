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

/*!\file
 * \brief This binary demonstrates how to build an executable with TensorFlow
 * Lite. A library for aomenc/aomdec is almost identical in structure,
 * except that "main" will not be present, and a header file will be added.
 */

#include <iostream>
#include <memory>

#include "common/tf_lite_includes.h"

namespace {

/**
 * Model data. Steps to create:
 *
 * 1. Build your model like normal.
 * 2. Follow the steps at https://www.tensorflow.org/lite/convert/python_api
 *    to convert into a TF-lite model. Optionally quantize the model to make
 *    it smaller (see the Quantization section at
 *    https://www.tensorflow.org/lite/guide/get_started)
 * 3. Run `xxd -i model.tflite > model.cc` to make it a CC file.
 * 4. Change the declaration to be const.
 * 5. Either include directly or create .h/.cc files that expose the array.
 */
const unsigned char tf_lite_model_data[] = {
  0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, 0x00, 0x00, 0x12, 0x00, 0x1c,
  0x00, 0x04, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x10, 0x00, 0x14, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x12, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xf4, 0x04, 0x00,
  0x00, 0x4c, 0x01, 0x00, 0x00, 0x34, 0x01, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xb0,
  0xfb, 0xff, 0xff, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x13, 0x00,
  0x00, 0x00, 0x6d, 0x69, 0x6e, 0x5f, 0x72, 0x75, 0x6e, 0x74, 0x69, 0x6d, 0x65,
  0x5f, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x0a, 0x00, 0x00, 0x00,
  0xf4, 0x00, 0x00, 0x00, 0xdc, 0x00, 0x00, 0x00, 0xac, 0x00, 0x00, 0x00, 0x5c,
  0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x2c, 0x00,
  0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
  0x00, 0xba, 0xfe, 0xff, 0xff, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
  0x31, 0x2e, 0x35, 0x2e, 0x30, 0x00, 0x00, 0x00, 0xb0, 0xfb, 0xff, 0xff, 0xb4,
  0xfb, 0xff, 0xff, 0xb8, 0xfb, 0xff, 0xff, 0xbc, 0xfb, 0xff, 0xff, 0xde, 0xfe,
  0xff, 0xff, 0x04, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x83, 0xad, 0x05,
  0x3e, 0x0a, 0x72, 0x13, 0x3f, 0x83, 0xfb, 0x88, 0x3f, 0x2a, 0xf1, 0x8b, 0xbe,
  0xfa, 0xfe, 0xff, 0xff, 0x04, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x86,
  0xc7, 0x46, 0x3f, 0x57, 0xb7, 0x35, 0xbf, 0x78, 0x2c, 0xeb, 0x3e, 0x25, 0xab,
  0xae, 0xbe, 0x93, 0xbe, 0x6f, 0x3f, 0x44, 0x91, 0x8d, 0x3e, 0xdc, 0x92, 0xe7,
  0x3e, 0x25, 0x67, 0x83, 0xbf, 0x2a, 0xc0, 0x7a, 0xbf, 0xf8, 0x46, 0x9d, 0x3f,
  0x0b, 0x18, 0x6b, 0xbf, 0x0d, 0xe7, 0xc1, 0x3e, 0xb4, 0xd9, 0x15, 0xbf, 0x8d,
  0x62, 0xa3, 0x3e, 0xb4, 0x5e, 0xab, 0xbb, 0xb8, 0x49, 0x3a, 0x3e, 0x46, 0xff,
  0xff, 0xff, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x5a, 0xdc, 0x3d,
  0x3d, 0xdd, 0x63, 0x17, 0xbe, 0x4a, 0x05, 0x1a, 0xbf, 0x27, 0xde, 0x3d, 0x3e,
  0x74, 0xcd, 0xd7, 0x3e, 0x63, 0xff, 0x0c, 0x3f, 0xf7, 0xcc, 0x0d, 0x3f, 0x10,
  0x30, 0x00, 0xbf, 0x72, 0xff, 0xff, 0xff, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00,
  0x00, 0x00, 0x76, 0xf6, 0x0a, 0xbf, 0x76, 0xf6, 0x0a, 0x3f, 0x68, 0xfc, 0xff,
  0xff, 0x0f, 0x00, 0x00, 0x00, 0x54, 0x4f, 0x43, 0x4f, 0x20, 0x43, 0x6f, 0x6e,
  0x76, 0x65, 0x72, 0x74, 0x65, 0x64, 0x2e, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10,
  0x00, 0x00, 0x00, 0x0c, 0x00, 0x14, 0x00, 0x04, 0x00, 0x08, 0x00, 0x0c, 0x00,
  0x10, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xf8, 0x00, 0x00,
  0x00, 0xec, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0xa4, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x0e, 0x00, 0x1a, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x10, 0x00, 0x07, 0x00,
  0x14, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x01, 0x00, 0x00,
  0x00, 0x24, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x06, 0x00, 0x08, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x80, 0x3f, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x14, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x0c, 0x00, 0x07, 0x00, 0x10, 0x00, 0x0e, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x08, 0x18, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x24, 0xfd, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00,
  0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x16, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x0c, 0x00, 0x07, 0x00, 0x10, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x08, 0x24, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x0c, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x08, 0x00, 0x07, 0x00, 0x06, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4c, 0x02, 0x00,
  0x00, 0xe4, 0x01, 0x00, 0x00, 0x9c, 0x01, 0x00, 0x00, 0x3c, 0x01, 0x00, 0x00,
  0xf4, 0x00, 0x00, 0x00, 0xac, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0xe2, 0xfd, 0xff, 0xff, 0x38, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xd4, 0xfd, 0xff,
  0xff, 0x1e, 0x00, 0x00, 0x00, 0x73, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x74, 0x69,
  0x61, 0x6c, 0x2f, 0x64, 0x65, 0x6e, 0x73, 0x65, 0x5f, 0x31, 0x2f, 0x4d, 0x61,
  0x74, 0x4d, 0x75, 0x6c, 0x5f, 0x62, 0x69, 0x61, 0x73, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x26, 0xfe, 0xff, 0xff, 0x4c, 0x00, 0x00,
  0x00, 0x02, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x18, 0xfe, 0xff, 0xff, 0x32, 0x00, 0x00, 0x00, 0x73, 0x65, 0x71, 0x75, 0x65,
  0x6e, 0x74, 0x69, 0x61, 0x6c, 0x2f, 0x64, 0x65, 0x6e, 0x73, 0x65, 0x5f, 0x31,
  0x2f, 0x4d, 0x61, 0x74, 0x4d, 0x75, 0x6c, 0x2f, 0x52, 0x65, 0x61, 0x64, 0x56,
  0x61, 0x72, 0x69, 0x61, 0x62, 0x6c, 0x65, 0x4f, 0x70, 0x2f, 0x74, 0x72, 0x61,
  0x6e, 0x73, 0x70, 0x6f, 0x73, 0x65, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x82, 0xfe, 0xff, 0xff, 0x34, 0x00,
  0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
  0x00, 0x74, 0xfe, 0xff, 0xff, 0x1a, 0x00, 0x00, 0x00, 0x73, 0x65, 0x71, 0x75,
  0x65, 0x6e, 0x74, 0x69, 0x61, 0x6c, 0x2f, 0x64, 0x65, 0x6e, 0x73, 0x65, 0x5f,
  0x31, 0x2f, 0x42, 0x69, 0x61, 0x73, 0x41, 0x64, 0x64, 0x00, 0x00, 0x02, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xc6, 0xfe, 0xff,
  0xff, 0x38, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0xb8, 0xfe, 0xff, 0xff, 0x1c, 0x00, 0x00, 0x00, 0x73,
  0x65, 0x71, 0x75, 0x65, 0x6e, 0x74, 0x69, 0x61, 0x6c, 0x2f, 0x64, 0x65, 0x6e,
  0x73, 0x65, 0x2f, 0x4d, 0x61, 0x74, 0x4d, 0x75, 0x6c, 0x5f, 0x62, 0x69, 0x61,
  0x73, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x0a, 0xff, 0xff, 0xff, 0x4c, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0c,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xfc, 0xfe, 0xff, 0xff, 0x30, 0x00,
  0x00, 0x00, 0x73, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x74, 0x69, 0x61, 0x6c, 0x2f,
  0x64, 0x65, 0x6e, 0x73, 0x65, 0x2f, 0x4d, 0x61, 0x74, 0x4d, 0x75, 0x6c, 0x2f,
  0x52, 0x65, 0x61, 0x64, 0x56, 0x61, 0x72, 0x69, 0x61, 0x62, 0x6c, 0x65, 0x4f,
  0x70, 0x2f, 0x74, 0x72, 0x61, 0x6e, 0x73, 0x70, 0x6f, 0x73, 0x65, 0x00, 0x00,
  0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
  0x00, 0x66, 0xff, 0xff, 0xff, 0x34, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x58, 0xff, 0xff, 0xff, 0x1a,
  0x00, 0x00, 0x00, 0x73, 0x65, 0x71, 0x75, 0x65, 0x6e, 0x74, 0x69, 0x61, 0x6c,
  0x2f, 0x61, 0x63, 0x74, 0x69, 0x76, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x52,
  0x65, 0x6c, 0x75, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0xaa, 0xff, 0xff, 0xff, 0x44, 0x00, 0x00, 0x00, 0x07,
  0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x08, 0x00,
  0x0c, 0x00, 0x04, 0x00, 0x08, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x43,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x64,
  0x65, 0x6e, 0x73, 0x65, 0x5f, 0x69, 0x6e, 0x70, 0x75, 0x74, 0x00, 0x02, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e,
  0x00, 0x14, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x10, 0x00,
  0x0e, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x10,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00,
  0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x49, 0x64, 0x65, 0x6e, 0x74, 0x69, 0x74,
  0x79, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x0c,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00, 0x05, 0x00, 0x06, 0x00,
  0x00, 0x00, 0x00, 0x19, 0x0a, 0x00, 0x0c, 0x00, 0x07, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x03, 0x00, 0x00, 0x00
};

/**
 * These lines were generated by building the binary
 * tensorflow/lite/tools/generate_op_registrations and running it with
 * the TF-Lite model as input. In order to build that binary, follow
 * these instructions:
 *
 * 1.) Check out a clean copy of tensorflow/ in a different directory. Pin to
 *     commit 18445b0e39b677a21c86b4cf3d2bcb823f27e3e2. Make sure you are
 *     in the root of the tensorflow/ directory.
 * 2.) Apply the following diff to
 *     tensorflow/lite/tools/gen_op_registration_main.cc:
   <pre>
153,155c153,155
<   std::string input_models;
<   std::string output_registration;
<   std::string tflite_path;
---
>   std::string input_model(argv[1]);
>   std::string output_registration(argv[2]);
>   std::string tflite_path("tensorflow/lite");
158,159d157
<   ParseFlagAndInit(&argc, argv, &input_models, &output_registration,
<                    &tflite_path, &namespace_flag, &for_micro);
163,171c161
<   if (!input_models.empty()) {
<     std::vector<std::string> models = absl::StrSplit(input_models, ',');
<     for (const std::string& input_model : models) {
<       AddOpsFromModel(input_model, &builtin_ops, &custom_ops);
<     }
<   }
<   for (int i = 1; i < argc; i++) {
<     AddOpsFromModel(argv[i], &builtin_ops, &custom_ops);
<   }
---
>   AddOpsFromModel(input_model, &builtin_ops, &custom_ops);
   </pre>
 * This diff fixes an issue with the current binary's flag parsing
 * library, which is failing silently.
 *
 * 3. Run "bazel build tensorflow/lite/tools/generate_op_registrations"
 * and then run:
   <pre>
 ./bazel-bin/tensorflow/lite/tools/generate_op_registrations \
    /path/to/model.tflite /path/to/output.cc
   </pre>
 * For appropriate paths to the input and output files.
 */
void RegisterSpecificOps(tflite::MutableOpResolver *resolver) {
  resolver->AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                       tflite::ops::builtin::Register_FULLY_CONNECTED(), 3, 3);
  resolver->AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                       tflite::ops::builtin::Register_SOFTMAX());
}

}  // namespace

int main() {
  // Toy model that takes in four floats and returns two classification
  // probabilities.
  auto model = tflite::GetModel(tf_lite_model_data);
  // Build the interpreter.
  tflite::MutableOpResolver resolver;
  RegisterSpecificOps(&resolver);
  tflite::InterpreterBuilder builder(model, resolver);
  std::unique_ptr<tflite::Interpreter> interpreter;
  builder(&interpreter);

  std::unique_ptr<tflite::ErrorReporter> reporter(
      tflite::DefaultErrorReporter());

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    reporter->Report("Failed");
    return EXIT_FAILURE;
  }

  // Always uses (0, 1, 2, 3) as inputs.
  float *input = interpreter->typed_input_tensor<float>(0);
  for (int i = 0; i < 4; ++i) {
    input[i] = i;
  }
  auto status = interpreter->Invoke();
  if (status != kTfLiteOk) {
    reporter->Report("Failed");
    return EXIT_FAILURE;
  }
  float y0 = interpreter->typed_output_tensor<float>(0)[0];
  float y1 = interpreter->typed_output_tensor<float>(0)[1];
  std::cout << "Sample classification probability: "
            << y0 << ", " << y1 << std::endl;
  return EXIT_SUCCESS;
}
