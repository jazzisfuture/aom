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

// Scalable Decoder
// ==============
//
// This is an example of a scalable decoder loop. It takes a 2-spatial-layer
// input file
// containing the compressed data (in OBU format), passes it through the
// decoder, and writes the decompressed frames to disk. The base layer and
// enhancement layers are stored as separate files, lyr0.yuv and lyr1.yuv,
// respectively.
//
// Standard Includes
// -----------------
// For decoders, you only have to include `aom_decoder.h` and then any
// header files for the specific codecs you use. In this case, we're using
// aom.
//
// Initializing The Codec
// ----------------------
// The libaom decoder is initialized by the call to aom_codec_dec_init().
// Determining the codec interface to use is handled by AvxVideoReader and the
// functions prefixed with aom_video_reader_. Discussion of those functions is
// beyond the scope of this example, but the main gist is to open the input file
// and parse just enough of it to determine if it's a AVx file and which AVx
// codec is contained within the file.
// Note the NULL pointer passed to aom_codec_dec_init(). We do that in this
// example because we want the algorithm to determine the stream configuration
// (width/height) and allocate memory automatically.
//
// Decoding A Frame
// ----------------
// Once the frame has been read into memory, it is decoded using the
// `aom_codec_decode` function. The call takes a pointer to the data
// (`frame`) and the length of the data (`frame_size`). No application data
// is associated with the frame in this example, so the `user_priv`
// parameter is NULL. The `deadline` parameter is left at zero for this
// example. This parameter is generally only used when doing adaptive post
// processing.
//
// Codecs may produce a variable number of output frames for every call to
// `aom_codec_decode`. These frames are retrieved by the
// `aom_codec_get_frame` iterator function. The iterator variable `iter` is
// initialized to NULL each time `aom_codec_decode` is called.
// `aom_codec_get_frame` is called in a loop, returning a pointer to a
// decoded image or NULL to indicate the end of list.
//
// Processing The Decoded Data
// ---------------------------
// In this example, we simply write the encoded data to disk. It is
// important to honor the image's `stride` values.
//
// Cleanup
// -------
// The `aom_codec_destroy` call frees any memory allocated by the codec.
//
// Error Handling
// --------------
// This example does not special case any error return codes. If there was
// an error, a descriptive message is printed and the program exits. With
// few exceptions, aom_codec functions return an enumerated error status,
// with the value `0` indicating success.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_decoder.h"

#include "../tools_common.h"
#include "../video_reader.h"
#include "./aom_config.h"
#include "./obudec.h"

static const char *exec_name;

struct AvxDecInputContext {
  struct AvxInputContext *aom_input_ctx;
  struct WebmInputContext *webm_ctx;
};

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <infile>\n", exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  int frame_cnt = 0;
  FILE *outfile0 = NULL;
  FILE *outfile1 = NULL;
  aom_codec_ctx_t codec;
  const AvxInterface *decoder = NULL;
  FILE *inputfile = NULL;
  uint8_t *buf = NULL;
  size_t bytes_in_buffer = 0, buffer_size = 0;
  struct AvxDecInputContext input = { NULL, NULL };
  struct AvxInputContext aom_input_ctx;
  input.aom_input_ctx = &aom_input_ctx;

  exec_name = argv[0];

  if (argc != 2) die("Invalid number of arguments.");

  if (!(inputfile = fopen(argv[1], "rb")))
    die("Failed to open %s for read.", argv[1]);
  input.aom_input_ctx->file = inputfile;

  if (!(outfile0 = fopen("lyr0.yuv", "wb")))
    die("Failed to open lyr0.yuv for writing.");
  if (!(outfile1 = fopen("lyr1.yuv", "wb")))
    die("Failed to open lyr1.yuv for writing.");

  if (!decoder) decoder = get_aom_decoder_by_index(0);

  printf("Using %s\n", aom_codec_iface_name(decoder->codec_interface()));

  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

#if CONFIG_SCALABILITY
  if (!file_is_obu(input.aom_input_ctx))
    die_codec(&codec, "Input is not a valid obu file");
#endif

#if CONFIG_SCALABILITY
  while (!obu_read_temporal_unit(inputfile, &buf, &bytes_in_buffer,
                                 &buffer_size)) {
#else
  while (1) {
#endif
    aom_codec_iter_t iter = NULL;
    aom_image_t *img = NULL;
    if (aom_codec_decode(&codec, buf, (unsigned int)bytes_in_buffer, NULL, 0))
      die_codec(&codec, "Failed to decode frame.");

    while ((img = aom_codec_get_frame(&codec, &iter)) != NULL) {
#if CONFIG_SCALABILITY
      if (img->enhancement_id == 0)
        aom_img_write(img, outfile0);
      else if (img->enhancement_id == 1)
        aom_img_write(img, outfile1);
#else
      aom_img_write(img, outfile0);
      die_codec(&codec, "CONFIG_SCALABILITY must be enabled.");
#endif
      ++frame_cnt;
    }
  }

  printf("Processed %d frames.\n", frame_cnt / 2);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");

  fclose(outfile0);
  fclose(outfile1);
  fclose(inputfile);

  return EXIT_SUCCESS;
}
