/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "examples/tf_lite_ops_registration.h"
#include "tensorflow/lite/kernels/builtin_op_kernels.h"
#include "tensorflow/lite/model.h"

void RegisterSpecificOps(tflite::MutableOpResolver *resolver) {
  // These lines were generated by building the binary
  // tensorflow/lite/tools/generate_op_registrations and running it with
  // the TF-Lite model as input. In order to build that binary:
  //
  //  1.) Check out a clean copy of tensorflow/ in a different directory.
  //  2.) Modify tensorflow/tensorflow.bzl's BUILD rule for tf_kernel_library to
  //      include "-cstd=c++14" in the copts.
  //  3.) Directly code in the variables "input_model", "output_registration",
  //      and "tflite_path" in
  //      tensorflow/lite/tools/gen_op_registration_main.cc, and delete the
  //      for-loop over argv; its flag parsing library is silently failing
  //      and not parsing any variables. I.e., after the ParseFlagAndInit
  //      statement, add:
  //          input_model = "/path/to/model.tflite";
  //          output_registration = "/path/to/output_file.cc";
  //          tflite_path = "tensorflow/lite";
  //          for_micro = false;
  // Run "bazel build tensorflow/lite/tools/generate_op_registrations"
  // and then run "bazel-bin/tensorflow/lite/tools/generate_op_registrations"
  resolver->AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                       tflite::ops::builtin::Register_FULLY_CONNECTED(), 3, 3);
  resolver->AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                       tflite::ops::builtin::Register_SOFTMAX());
}
