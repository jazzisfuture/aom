/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <string.h>
#include <assert.h>
#include "av1/encoder/encoder.h"
#include "av1/encoder/encoder_utils.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/saliency_map.h"

// The Gabor filter is generated by setting the parameters as:
// ksize = 9
// sigma = 1
// theta = y*np.pi/4, where y /in {0, 1, 2, 3}, i.e., 0, 45, 90, 135 degree
// lambda1 = 1
// gamma=0.8
// phi =0
static const double kGaborFilter[4][9][9] = {  // [angle: 0, 45, 90, 135
                                               // degree][ksize][ksize]
  { { 2.0047323e-06, 6.6387620e-05, 8.0876675e-04, 3.6246411e-03, 5.9760227e-03,
      3.6246411e-03, 8.0876675e-04, 6.6387620e-05, 2.0047323e-06 },
    { 1.8831115e-05, 6.2360091e-04, 7.5970138e-03, 3.4047455e-02, 5.6134764e-02,
      3.4047455e-02, 7.5970138e-03, 6.2360091e-04, 1.8831115e-05 },
    { 9.3271126e-05, 3.0887155e-03, 3.7628256e-02, 1.6863814e-01, 2.7803731e-01,
      1.6863814e-01, 3.7628256e-02, 3.0887155e-03, 9.3271126e-05 },
    { 2.4359586e-04, 8.0667874e-03, 9.8273583e-02, 4.4043165e-01, 7.2614902e-01,
      4.4043165e-01, 9.8273583e-02, 8.0667874e-03, 2.4359586e-04 },
    { 3.3546262e-04, 1.1108996e-02, 1.3533528e-01, 6.0653067e-01, 1.0000000e+00,
      6.0653067e-01, 1.3533528e-01, 1.1108996e-02, 3.3546262e-04 },
    { 2.4359586e-04, 8.0667874e-03, 9.8273583e-02, 4.4043165e-01, 7.2614902e-01,
      4.4043165e-01, 9.8273583e-02, 8.0667874e-03, 2.4359586e-04 },
    { 9.3271126e-05, 3.0887155e-03, 3.7628256e-02, 1.6863814e-01, 2.7803731e-01,
      1.6863814e-01, 3.7628256e-02, 3.0887155e-03, 9.3271126e-05 },
    { 1.8831115e-05, 6.2360091e-04, 7.5970138e-03, 3.4047455e-02, 5.6134764e-02,
      3.4047455e-02, 7.5970138e-03, 6.2360091e-04, 1.8831115e-05 },
    { 2.0047323e-06, 6.6387620e-05, 8.0876675e-04, 3.6246411e-03, 5.9760227e-03,
      3.6246411e-03, 8.0876675e-04, 6.6387620e-05, 2.0047323e-06 } },

  { { -6.2165498e-08, 3.8760313e-06, 3.0079011e-06, -4.4602581e-04,
      6.6981313e-04, 1.3962291e-03, -9.9486928e-04, -8.1631159e-05,
      3.5712848e-05 },
    { 3.8760313e-06, 5.7044272e-06, -1.6041942e-03, 4.5687673e-03,
      1.8061366e-02, -2.4406660e-02, -3.7979286e-03, 3.1511115e-03,
      -8.1631159e-05 },
    { 3.0079011e-06, -1.6041942e-03, 8.6645801e-03, 6.4960226e-02,
      -1.6647682e-01, -4.9129307e-02, 7.7304743e-02, -3.7979286e-03,
      -9.9486928e-04 },
    { -4.4602581e-04, 4.5687673e-03, 6.4960226e-02, -3.1572008e-01,
      -1.7670043e-01, 5.2729243e-01, -4.9129307e-02, -2.4406660e-02,
      1.3962291e-03 },
    { 6.6981313e-04, 1.8061366e-02, -1.6647682e-01, -1.7670043e-01,
      1.0000000e+00, -1.7670043e-01, -1.6647682e-01, 1.8061366e-02,
      6.6981313e-04 },
    { 1.3962291e-03, -2.4406660e-02, -4.9129307e-02, 5.2729243e-01,
      -1.7670043e-01, -3.1572008e-01, 6.4960226e-02, 4.5687673e-03,
      -4.4602581e-04 },
    { -9.9486928e-04, -3.7979286e-03, 7.7304743e-02, -4.9129307e-02,
      -1.6647682e-01, 6.4960226e-02, 8.6645801e-03, -1.6041942e-03,
      3.0079011e-06 },
    { -8.1631159e-05, 3.1511115e-03, -3.7979286e-03, -2.4406660e-02,
      1.8061366e-02, 4.5687673e-03, -1.6041942e-03, 5.7044272e-06,
      3.8760313e-06 },
    { 3.5712848e-05, -8.1631159e-05, -9.9486928e-04, 1.3962291e-03,
      6.6981313e-04, -4.4602581e-04, 3.0079011e-06, 3.8760313e-06,
      -6.2165498e-08 } },

  { { 2.0047323e-06, 1.8831115e-05, 9.3271126e-05, 2.4359586e-04, 3.3546262e-04,
      2.4359586e-04, 9.3271126e-05, 1.8831115e-05, 2.0047323e-06 },
    { 6.6387620e-05, 6.2360091e-04, 3.0887155e-03, 8.0667874e-03, 1.1108996e-02,
      8.0667874e-03, 3.0887155e-03, 6.2360091e-04, 6.6387620e-05 },
    { 8.0876675e-04, 7.5970138e-03, 3.7628256e-02, 9.8273583e-02, 1.3533528e-01,
      9.8273583e-02, 3.7628256e-02, 7.5970138e-03, 8.0876675e-04 },
    { 3.6246411e-03, 3.4047455e-02, 1.6863814e-01, 4.4043165e-01, 6.0653067e-01,
      4.4043165e-01, 1.6863814e-01, 3.4047455e-02, 3.6246411e-03 },
    { 5.9760227e-03, 5.6134764e-02, 2.7803731e-01, 7.2614902e-01, 1.0000000e+00,
      7.2614902e-01, 2.7803731e-01, 5.6134764e-02, 5.9760227e-03 },
    { 3.6246411e-03, 3.4047455e-02, 1.6863814e-01, 4.4043165e-01, 6.0653067e-01,
      4.4043165e-01, 1.6863814e-01, 3.4047455e-02, 3.6246411e-03 },
    { 8.0876675e-04, 7.5970138e-03, 3.7628256e-02, 9.8273583e-02, 1.3533528e-01,
      9.8273583e-02, 3.7628256e-02, 7.5970138e-03, 8.0876675e-04 },
    { 6.6387620e-05, 6.2360091e-04, 3.0887155e-03, 8.0667874e-03, 1.1108996e-02,
      8.0667874e-03, 3.0887155e-03, 6.2360091e-04, 6.6387620e-05 },
    { 2.0047323e-06, 1.8831115e-05, 9.3271126e-05, 2.4359586e-04, 3.3546262e-04,
      2.4359586e-04, 9.3271126e-05, 1.8831115e-05, 2.0047323e-06 } },

  { { 3.5712848e-05, -8.1631159e-05, -9.9486928e-04, 1.3962291e-03,
      6.6981313e-04, -4.4602581e-04, 3.0079011e-06, 3.8760313e-06,
      -6.2165498e-08 },
    { -8.1631159e-05, 3.1511115e-03, -3.7979286e-03, -2.4406660e-02,
      1.8061366e-02, 4.5687673e-03, -1.6041942e-03, 5.7044272e-06,
      3.8760313e-06 },
    { -9.9486928e-04, -3.7979286e-03, 7.7304743e-02, -4.9129307e-02,
      -1.6647682e-01, 6.4960226e-02, 8.6645801e-03, -1.6041942e-03,
      3.0079011e-06 },
    { 1.3962291e-03, -2.4406660e-02, -4.9129307e-02, 5.2729243e-01,
      -1.7670043e-01, -3.1572008e-01, 6.4960226e-02, 4.5687673e-03,
      -4.4602581e-04 },
    { 6.6981313e-04, 1.8061366e-02, -1.6647682e-01, -1.7670043e-01,
      1.0000000e+00, -1.7670043e-01, -1.6647682e-01, 1.8061366e-02,
      6.6981313e-04 },
    { -4.4602581e-04, 4.5687673e-03, 6.4960226e-02, -3.1572008e-01,
      -1.7670043e-01, 5.2729243e-01, -4.9129307e-02, -2.4406660e-02,
      1.3962291e-03 },
    { 3.0079011e-06, -1.6041942e-03, 8.6645801e-03, 6.4960226e-02,
      -1.6647682e-01, -4.9129307e-02, 7.7304743e-02, -3.7979286e-03,
      -9.9486928e-04 },
    { 3.8760313e-06, 5.7044272e-06, -1.6041942e-03, 4.5687673e-03,
      1.8061366e-02, -2.4406660e-02, -3.7979286e-03, 3.1511115e-03,
      -8.1631159e-05 },
    { -6.2165498e-08, 3.8760313e-06, 3.0079011e-06, -4.4602581e-04,
      6.6981313e-04, 1.3962291e-03, -9.9486928e-04, -8.1631159e-05,
      3.5712848e-05 } }
};

// This function is to extract red/green/blue channels, and calculate intensity
// = (r+g+b)/3. Note that it only handles 8bits case now.
// TODO(linzhen): add high bitdepth support.
static void get_color_intensity(const YV12_BUFFER_CONFIG *src,
                                int subsampling_x, int subsampling_y,
                                double *cr, double *cg, double *cb,
                                double *intensity) {
  const uint8_t *y = src->buffers[0];
  const uint8_t *u = src->buffers[1];
  const uint8_t *v = src->buffers[2];

  const int y_height = src->crop_heights[0];
  const int y_width = src->crop_widths[0];
  const int y_stride = src->strides[0];
  const int c_stride = src->strides[1];

  for (int i = 0; i < y_height; i++) {
    for (int j = 0; j < y_width; j++) {
      cr[i * y_width + j] =
          fclamp((double)y[i * y_stride + j] +
                     1.370 * (double)(v[(i >> subsampling_y) * c_stride +
                                        (j >> subsampling_x)] -
                                      128),
                 0, 255);
      cg[i * y_width + j] =
          fclamp((double)y[i * y_stride + j] -
                     0.698 * (double)(u[(i >> subsampling_y) * c_stride +
                                        (j >> subsampling_x)] -
                                      128) -
                     0.337 * (double)(v[(i >> subsampling_y) * c_stride +
                                        (j >> subsampling_x)] -
                                      128),
                 0, 255);
      cb[i * y_width + j] =
          fclamp((double)y[i * y_stride + j] +
                     1.732 * (double)(u[(i >> subsampling_y) * c_stride +
                                        (j >> subsampling_x)] -
                                      128),
                 0, 255);

      intensity[i * y_width + j] =
          (cr[i * y_width + j] + cg[i * y_width + j] + cb[i * y_width + j]) /
          3.0;
      assert(intensity[i * y_width + j] >= 0 &&
             intensity[i * y_width + j] <= 255);

      intensity[i * y_width + j] /= 256;
      cr[i * y_width + j] /= 256;
      cg[i * y_width + j] /= 256;
      cb[i * y_width + j] /= 256;
    }
  }
}

static INLINE double convolve_map(const double *filter, const double *map,
                                  const int size) {
  double result = 0;
  for (int i = 0; i < size; i++) {
    result += filter[i] * map[i];  // symmetric filter is used
  }
  return result;
}

// This function is to decimate the map by half, and apply Gaussian filter on
// top of the reduced map.
static INLINE void decimate_map(const double *map, int height, int width,
                                int stride, double *reduced_map) {
  const int new_width = width / 2;
  const int window_size = 5;
  const double gaussian_filter[25] = {
    1. / 256, 1.0 / 64, 3. / 128, 1. / 64,  1. / 256, 1. / 64, 1. / 16,
    3. / 32,  1. / 16,  1. / 64,  3. / 128, 3. / 32,  9. / 64, 3. / 32,
    3. / 128, 1. / 64,  1. / 16,  3. / 32,  1. / 16,  1. / 64, 1. / 256,
    1. / 64,  3. / 128, 1. / 64,  1. / 256
  };

  double map_region[25];
  for (int y = 0; y < height - 1; y += 2) {
    for (int x = 0; x < width - 1; x += 2) {
      int i = 0;
      for (int yy = y - window_size / 2; yy <= y + window_size / 2; yy++) {
        for (int xx = x - window_size / 2; xx <= x + window_size / 2; xx++) {
          int yvalue = clamp(yy, 0, height - 1);
          int xvalue = clamp(xx, 0, width - 1);
          map_region[i++] = map[yvalue * stride + xvalue];
        }
      }
      reduced_map[(y / 2) * new_width + (x / 2)] =
          convolve_map(gaussian_filter, map_region, window_size * window_size);
    }
  }
}

// This function is to upscale the map from in_level size to out_level size.
// Note that the map at "level-1" will upscale the map at "level" by x2.
static INLINE int upscale_map(const double *input, int in_level, int out_level,
                              int height[9], int width[9], double *output) {
  for (int level = in_level; level > out_level; level--) {
    const int cur_width = width[level];
    const int cur_height = height[level];
    const int cur_stride = width[level];

    double *original = (level == in_level) ? (double *)input : output;

    assert(level > 0);

    int h_upscale = height[level - 1];
    int w_upscale = width[level - 1];
    int s_upscale = width[level - 1];

    double *upscale = aom_malloc(h_upscale * w_upscale * sizeof(*upscale));

    if (!upscale) {
      return 0;
    }

    for (int i = 0; i < h_upscale; ++i) {
      for (int j = 0; j < w_upscale; ++j) {
        const int ii = clamp((i >> 1), 0, cur_height - 1);
        const int jj = clamp((j >> 1), 0, cur_width - 1);

        upscale[j + i * s_upscale] = (double)original[jj + ii * cur_stride];
      }
    }
    memcpy(output, upscale, h_upscale * w_upscale * sizeof(double));
    aom_free(upscale);
  }

  return 1;
}

// This function calculates the differences between a fine scale c and a
// coarser scale s yielding the feature maps. c \in {2, 3, 4}, and s = c +
// delta, where delta \in {3, 4}.
static int center_surround_diff(const double *input[9], int height[9],
                                int width[9], saliency_feature_map *output[6]) {
  int j = 0;
  for (int k = 2; k < 5; k++) {
    int cur_height = height[k];
    int cur_width = width[k];

    output[j]->buf =
        (double *)aom_malloc(cur_height * cur_width * sizeof(*output[j]->buf));
    output[j + 1]->buf = (double *)aom_malloc(cur_height * cur_width *
                                              sizeof(*output[j + 1]->buf));

    if (!output[j]->buf || !output[j + 1]->buf) {
      aom_free(output[j]->buf);
      aom_free(output[j + 1]->buf);
      return 0;
    }

    output[j]->height = output[j + 1]->height = cur_height;
    output[j]->width = output[j + 1]->width = cur_width;

    if (upscale_map(input[k + 3], k + 3, k, height, width, output[j]->buf) ==
        0) {
      return 0;
    }

    for (int r = 0; r < cur_height; r++) {
      for (int c = 0; c < cur_width; c++) {
        output[j]->buf[r * cur_width + c] =
            fabs((double)(input[k][r * cur_width + c] -
                          output[j]->buf[r * cur_width + c]));
      }
    }

    if (upscale_map(input[k + 4], k + 4, k, height, width,
                    output[j + 1]->buf) == 0) {
      return 0;
    }

    for (int r = 0; r < cur_height; r++) {
      for (int c = 0; c < cur_width; c++) {
        output[j + 1]->buf[r * cur_width + c] =
            fabs(input[k][r * cur_width + c] -
                 output[j + 1]->buf[r * cur_width + c]);
      }
    }

    j += 2;
  }
  return 1;
}

// For color channels, the differences is calculated based on "color
// double-opponency". For example, the RG feature map is constructed between a
// fine scale c of R-G component and a coarser scale s of G-R component.
static int center_surround_diff_rgb(const double *input_1[9],
                                    const double *input_2[9], int height[9],
                                    int width[9],
                                    saliency_feature_map *output[6]) {
  int j = 0;
  for (int k = 2; k < 5; k++) {
    int cur_height = height[k];
    int cur_width = width[k];

    output[j]->buf =
        (double *)aom_malloc(cur_height * cur_width * sizeof(*output[j]->buf));
    output[j + 1]->buf = (double *)aom_malloc(cur_height * cur_width *
                                              sizeof(*output[j + 1]->buf));

    if (!output[j]->buf || !output[j + 1]->buf) {
      aom_free(output[j]->buf);
      aom_free(output[j + 1]->buf);
      return 0;
    }

    output[j]->height = output[j + 1]->height = cur_height;
    output[j]->width = output[j + 1]->width = cur_width;

    if (upscale_map(input_2[k + 3], k + 3, k, height, width, output[j]->buf) ==
        0) {
      return 0;
    }

    for (int r = 0; r < cur_height; r++) {
      for (int c = 0; c < cur_width; c++) {
        output[j]->buf[r * cur_width + c] =
            fabs((double)(input_1[k][r * cur_width + c] -
                          output[j]->buf[r * cur_width + c]));
      }
    }

    if (upscale_map(input_2[k + 4], k + 4, k, height, width,
                    output[j + 1]->buf) == 0) {
      return 0;
    }

    for (int r = 0; r < cur_height; r++) {
      for (int c = 0; c < cur_width; c++) {
        output[j + 1]->buf[r * cur_width + c] =
            fabs(input_1[k][r * cur_width + c] -
                 output[j + 1]->buf[r * cur_width + c]);
      }
    }

    j += 2;
  }
  return 1;
}

// This function is to generate Gaussian pyramid images with indexes from 0 to
// 8, and construct the feature maps from calculating the center-surround
// differences.
static int gaussian_pyramid(const double *src, int width, int height,
                            saliency_feature_map *dst[6]) {
  double *gaussian_map[9];  // scale = 9
  int pyr_width[9];
  int pyr_height[9];

  gaussian_map[0] =
      (double *)aom_malloc(width * height * sizeof(*gaussian_map[0]));

  if (!gaussian_map[0]) {
    return 0;
  }

  memcpy(gaussian_map[0], src, width * height * sizeof(double));

  pyr_width[0] = width;
  pyr_height[0] = height;

  for (int i = 1; i < 9; i++) {
    int stride = pyr_width[i - 1];
    int new_width = pyr_width[i - 1] / 2;
    int new_height = pyr_height[i - 1] / 2;

    gaussian_map[i] =
        (double *)aom_malloc(new_width * new_height * sizeof(*gaussian_map[i]));

    if (!gaussian_map[i]) {
      return 0;
    }

    memset(gaussian_map[i], 0, new_width * new_height * sizeof(double));

    decimate_map(gaussian_map[i - 1], pyr_height[i - 1], pyr_width[i - 1],
                 stride, gaussian_map[i]);

    pyr_width[i] = new_width;
    pyr_height[i] = new_height;
  }

  if (center_surround_diff((const double **)gaussian_map, pyr_height, pyr_width,
                           dst) == 0) {
    return 0;
  }

  for (int i = 0; i < 9; i++) {
    aom_free(gaussian_map[i]);
  }
  return 1;
}

static int gaussian_pyramid_rgb(double *src_1, double *src_2, int width,
                                int height, saliency_feature_map *dst[6]) {
  double *gaussian_map[2][9];  // scale = 9
  int pyr_width[9];
  int pyr_height[9];
  double *src[2];

  src[0] = src_1;
  src[1] = src_2;

  for (int k = 0; k < 2; k++) {
    gaussian_map[k][0] =
        (double *)aom_malloc(width * height * sizeof(*gaussian_map[k][0]));
    if (!gaussian_map[k][0]) {
      return 0;
    }
    memcpy(gaussian_map[k][0], src[k], width * height * sizeof(double));

    pyr_width[0] = width;
    pyr_height[0] = height;

    for (int i = 1; i < 9; i++) {
      int stride = pyr_width[i - 1];
      int new_width = pyr_width[i - 1] / 2;
      int new_height = pyr_height[i - 1] / 2;

      gaussian_map[k][i] = (double *)aom_malloc(new_width * new_height *
                                                sizeof(*gaussian_map[k][i]));
      if (!gaussian_map[k][i]) {
        return 0;
      }
      memset(gaussian_map[k][i], 0, new_width * new_height * sizeof(double));
      decimate_map(gaussian_map[k][i - 1], pyr_height[i - 1], pyr_width[i - 1],
                   stride, gaussian_map[k][i]);

      pyr_width[i] = new_width;
      pyr_height[i] = new_height;
    }
  }

  if (center_surround_diff_rgb((const double **)gaussian_map[0],
                               (const double **)gaussian_map[1], pyr_height,
                               pyr_width, dst) == 0) {
    return 0;
  }

  for (int i = 0; i < 9; i++) {
    aom_free(gaussian_map[0][i]);
    aom_free(gaussian_map[1][i]);
  }
  return 1;
}

static int get_feature_map_intensity(double *intensity, int width, int height,
                                     saliency_feature_map *i_map[6]) {
  if (gaussian_pyramid(intensity, width, height, i_map) == 0) {
    return 0;
  }
  return 1;
}

static int get_feature_map_rgb(double *cr, double *cg, double *cb, int width,
                               int height, saliency_feature_map *rg_map[6],
                               saliency_feature_map *by_map[6]) {
  double *rg_mat = aom_malloc(height * width * sizeof(*rg_mat));
  double *by_mat = aom_malloc(height * width * sizeof(*by_mat));
  double *gr_mat = aom_malloc(height * width * sizeof(*gr_mat));
  double *yb_mat = aom_malloc(height * width * sizeof(*yb_mat));

  if (!rg_mat || !by_mat || !gr_mat || !yb_mat) {
    aom_free(rg_mat);
    aom_free(by_mat);
    aom_free(gr_mat);
    aom_free(yb_mat);
    return 0;
  }

  double r, g, b, y;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      r = AOMMAX(
          0, cr[i * width + j] - (cg[i * width + j] + cb[i * width + j]) / 2);
      g = AOMMAX(
          0, cg[i * width + j] - (cr[i * width + j] + cb[i * width + j]) / 2);
      b = AOMMAX(
          0, cb[i * width + j] - (cr[i * width + j] + cg[i * width + j]) / 2);
      y = AOMMAX(0, (cr[i * width + j] + cg[i * width + j]) / 2 -
                        fabs(cr[i * width + j] - cg[i * width + j]) / 2 -
                        cb[i * width + j]);

      rg_mat[i * width + j] = r - g;
      by_mat[i * width + j] = b - y;
      gr_mat[i * width + j] = g - r;
      yb_mat[i * width + j] = y - b;
    }
  }

  if (gaussian_pyramid_rgb(rg_mat, gr_mat, width, height, rg_map) == 0) {
    return 0;
  }
  if (gaussian_pyramid_rgb(by_mat, yb_mat, width, height, by_map) == 0) {
    return 0;
  }

  aom_free(rg_mat);
  aom_free(by_mat);
  aom_free(gr_mat);
  aom_free(yb_mat);
  return 1;
}

static INLINE void filter2d(const double *input, const double kernel[9][9],
                            int width, int height, double *output) {
  const int window_size = 9;
  double img_section[81];
  for (int y = 0; y <= height - 1; y++) {
    for (int x = 0; x <= width - 1; x++) {
      int i = 0;
      for (int yy = y - window_size / 2; yy <= y + window_size / 2; yy++) {
        for (int xx = x - window_size / 2; xx <= x + window_size / 2; xx++) {
          int yvalue = clamp(yy, 0, height - 1);
          int xvalue = clamp(xx, 0, width - 1);
          img_section[i++] = input[yvalue * width + xvalue];
        }
      }

      output[y * width + x] = 0;
      for (int k = 0; k < window_size; k++) {
        for (int l = 0; l < window_size; l++) {
          output[y * width + x] +=
              kernel[k][l] * img_section[k * window_size + l];
        }
      }
    }
  }
}

static int get_feature_map_orientation(const double *intensity, int width,
                                       int height,
                                       saliency_feature_map *dst[24]) {
  double *gaussian_map[9];
  int pyr_width[9];
  int pyr_height[9];

  gaussian_map[0] =
      (double *)aom_malloc(width * height * sizeof(*gaussian_map[0]));
  if (!gaussian_map[0]) {
    return 0;
  }
  memcpy(gaussian_map[0], intensity, width * height * sizeof(double));

  pyr_width[0] = width;
  pyr_height[0] = height;

  for (int i = 1; i < 9; i++) {
    int stride = pyr_width[i - 1];
    int new_width = pyr_width[i - 1] / 2;
    int new_height = pyr_height[i - 1] / 2;

    gaussian_map[i] =
        (double *)aom_malloc(new_width * new_height * sizeof(*gaussian_map[i]));
    if (!gaussian_map[i]) {
      return 0;
    }
    memset(gaussian_map[i], 0, new_width * new_height * sizeof(double));
    decimate_map(gaussian_map[i - 1], pyr_height[i - 1], pyr_width[i - 1],
                 stride, gaussian_map[i]);

    pyr_width[i] = new_width;
    pyr_height[i] = new_height;
  }

  double *tempGaborOutput[4][9];  //[angle: 0, 45, 90, 135 degree][filter_size]

  for (int i = 2; i < 9; i++) {
    const int cur_height = pyr_height[i];
    const int cur_width = pyr_width[i];
    for (int j = 0; j < 4; j++) {
      tempGaborOutput[j][i] = (double *)aom_malloc(
          cur_height * cur_width * sizeof(*tempGaborOutput[j][i]));
      if (!tempGaborOutput[j][i]) {
        return 0;
      }
      filter2d(gaussian_map[i], kGaborFilter[j], cur_width, cur_height,
               tempGaborOutput[j][i]);
    }
  }

  for (int i = 0; i < 9; i++) {
    aom_free(gaussian_map[i]);
  }

  saliency_feature_map
      *tmp[4][6];  //[angle: 0, 45, 90, 135 degree][filter_size]

  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 4; j++) {
      tmp[j][i] = (saliency_feature_map *)aom_malloc(sizeof(*tmp[j][i]));
      if (!tmp[j][i]) {
        return 0;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    if (center_surround_diff((const double **)tempGaborOutput[j], pyr_height,
                             pyr_width, tmp[j]) == 0) {
      return 0;
    }
  }

  for (int i = 2; i < 9; i++) {
    for (int j = 0; j < 4; j++) {
      aom_free(tempGaborOutput[j][i]);
    }
  }

  for (int i = 0; i < 6; i++) {
    dst[i] = tmp[0][i];
    dst[i + 6] = tmp[1][i];
    dst[i + 12] = tmp[2][i];
    dst[i + 18] = tmp[3][i];
  }

  return 1;
}

static INLINE void find_min_max(const saliency_feature_map *input,
                                double *max_value, double *min_value) {
  assert(input && input->buf);
  *min_value = 1;
  *max_value = 0;

  for (int i = 0; i < input->height; i++) {
    for (int j = 0; j < input->width; j++) {
      *min_value = fmin(input->buf[i * input->width + j], *min_value);
      *max_value = fmax(input->buf[i * input->width + j], *max_value);
    }
  }
}

static INLINE double average_local_max(const saliency_feature_map *input,
                                       int stepsize) {
  int numlocal = 0;
  double lmaxmean = 0, lmax = 0, dummy = 0;
  saliency_feature_map local_map;
  local_map.height = stepsize;
  local_map.width = stepsize;
  local_map.buf =
      (double *)aom_malloc(stepsize * stepsize * sizeof(*local_map.buf));

  if (!local_map.buf) {
    return -1;
  }

  for (int y = 0; y < input->height - stepsize; y += stepsize) {
    for (int x = 0; x < input->width - stepsize; x += stepsize) {
      for (int i = 0; i < stepsize; i++) {
        for (int j = 0; j < stepsize; j++) {
          local_map.buf[i * stepsize + j] =
              input->buf[(y + i) * input->width + x + j];
        }
      }

      find_min_max(&local_map, &lmax, &dummy);
      lmaxmean += lmax;
      numlocal++;
    }
  }

  aom_free(local_map.buf);

  return lmaxmean / numlocal;
}

// Linear normalization the values in the map to [0,1].
static saliency_feature_map *minmax_normalize(saliency_feature_map *input) {
  double max_value, min_value;

  find_min_max(input, &max_value, &min_value);

  saliency_feature_map *result =
      (saliency_feature_map *)aom_malloc(sizeof(*result));
  if (!result) {
    return NULL;
  }
  result->buf =
      (double *)aom_malloc(input->width * input->height * sizeof(*result->buf));
  if (!result->buf) {
    return NULL;
  }
  result->height = input->height;
  result->width = input->width;
  memset(result->buf, 0, input->width * input->height * sizeof(double));

  for (int i = 0; i < input->height; i++) {
    for (int j = 0; j < input->width; j++) {
      if (max_value != min_value) {
        result->buf[i * input->width + j] =
            input->buf[i * input->width + j] / (max_value - min_value) +
            min_value / (min_value - max_value);
      } else {
        result->buf[i * input->width + j] =
            input->buf[i * input->width + j] - min_value;
      }
    }
  }

  return result;
}

// This function is to promote meaningful “activation spots” in the map and
// ignores homogeneous areas.
static saliency_feature_map *nomalization_operator(saliency_feature_map *input,
                                                   int stepsize,
                                                   bool free_input) {
  saliency_feature_map *result =
      (saliency_feature_map *)aom_malloc(sizeof(*result));

  if (!result) {
    return NULL;
  }

  result->buf =
      (double *)aom_malloc(input->width * input->height * sizeof(*result->buf));
  if (!result->buf) {
    return NULL;
  }
  result->height = input->height;
  result->width = input->width;

  saliency_feature_map *temp_result = minmax_normalize(input);
  if (temp_result == NULL) {
    return NULL;
  }
  double lmaxmean = average_local_max(temp_result, stepsize);
  if (lmaxmean < 0) {
    return NULL;
  }
  double normCoeff = (1 - lmaxmean) * (1 - lmaxmean);

  for (int i = 0; i < input->height; i++) {
    for (int j = 0; j < input->width; j++) {
      result->buf[i * input->width + j] =
          temp_result->buf[i * input->width + j] * normCoeff;
    }
  }

  aom_free(temp_result->buf);
  aom_free(temp_result);

  if (free_input) {
    aom_free(input->buf);
    aom_free(input);
  }

  return result;
}

// Normalize the values in feature maps to [0,1], and then upscale all maps to
// the original frame size.
static int normalize_fm(saliency_feature_map *input[6], int width, int height,
                        int num_fm, saliency_feature_map *output[6]) {
  int pyr_height[9];
  int pyr_width[9];

  pyr_height[0] = height;
  pyr_width[0] = width;

  for (int i = 1; i < 9; i++) {
    int new_width = pyr_width[i - 1] / 2;
    int new_height = pyr_height[i - 1] / 2;

    pyr_width[i] = new_width;
    pyr_height[i] = new_height;
  }

  double *tmp = aom_malloc(width * height * sizeof(*tmp));
  if (!tmp) {
    return 0;
  }

  // Feature maps (FM) are generated by function "center_surround_diff()". The
  // difference is between a fine scale c and a coarser scale s, where c \in {2,
  // 3, 4}, and s = c + delta, where delta \in {3, 4}, and the FM size is scale
  // c. Specifically, i=0: c=2 and s=5, i=1: c=2 and s=6, i=2: c=3 and s=6, i=3:
  // c=3 and s=7, i=4: c=4 and s=7, i=5: c=4 and s=8.
  for (int i = 0; i < num_fm; i++) {
    // Normalization
    saliency_feature_map *normalizedmatrix =
        nomalization_operator(input[i], 8, false);
    if (normalizedmatrix == NULL) {
      return 0;
    }
    output[i]->buf =
        (double *)aom_malloc(width * height * sizeof(*output[i]->buf));
    if (!output[i]->buf) {
      return 0;
    }
    output[i]->height = height;
    output[i]->width = width;
    // Upscale FM to original frame size
    if (upscale_map(normalizedmatrix->buf, (i / 2) + 2, 0, pyr_height,
                    pyr_width, tmp) == 0) {
      return 0;
    }

    memcpy(output[i]->buf, (double *)tmp, width * height * sizeof(double));
    aom_free(normalizedmatrix->buf);
  }
  aom_free(tmp);
  return 1;
}

// Combine feature maps with the same category (intensity, color, or
// orientation) into one conspicuity map.
static saliency_feature_map *normalized_map(saliency_feature_map *input[6],
                                            int width, int height) {
  int num_fm = 6;

  saliency_feature_map *n_input[6];
  for (int i = 0; i < 6; i++) {
    n_input[i] = (saliency_feature_map *)aom_malloc(sizeof(*n_input[i]));
    if (!n_input[i]) {
      return NULL;
    }
  }
  if (normalize_fm(input, width, height, num_fm, n_input) == 0) {
    return NULL;
  }

  saliency_feature_map *output = aom_malloc(sizeof(*output));
  if (!output) {
    return NULL;
  }
  output->buf = (double *)aom_malloc(width * height * sizeof(*output->buf));
  if (!output->buf) {
    return NULL;
  }
  output->height = height;
  output->width = width;
  memset(output->buf, 0, width * height * sizeof(double));

  // Add up all normalized feature maps with the same category into one map.
  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++) {
      for (int i = 0; i < num_fm; i++) {
        output->buf[r * width + c] += n_input[i]->buf[r * width + c];
      }
    }
  }

  for (int i = 0; i < num_fm; i++) {
    aom_free(n_input[i]->buf);
    aom_free(n_input[i]);
  }

  return nomalization_operator(output, 8, true);
}

static saliency_feature_map *normalized_map_rgb(saliency_feature_map *rg_map[6],
                                                saliency_feature_map *by_map[6],
                                                int width, int height) {
  saliency_feature_map *color_cm_rg = normalized_map(rg_map, width, height);
  saliency_feature_map *color_cm_by = normalized_map(by_map, width, height);
  if (!color_cm_rg || !color_cm_by) {
    return NULL;
  }

  saliency_feature_map *color_cm = aom_malloc(sizeof(*color_cm));
  if (!color_cm) {
    return NULL;
  }
  color_cm->buf = (double *)aom_malloc(width * height * sizeof(*color_cm->buf));
  if (!color_cm->buf) {
    return NULL;
  }
  color_cm->height = height;
  color_cm->width = width;

  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++) {
      color_cm->buf[r * width + c] =
          color_cm_rg->buf[r * width + c] + color_cm_by->buf[r * width + c];
    }
  }

  aom_free(color_cm_rg->buf);
  aom_free(color_cm_by->buf);
  aom_free(color_cm_rg);
  aom_free(color_cm_by);

  return nomalization_operator(color_cm, 8, true);
}

static saliency_feature_map *normalized_map_orientation(
    saliency_feature_map *orientation_map[24], int width, int height) {
  int num_fms_per_angle = 6;

  saliency_feature_map *ofm[4][6];

  for (int i = 0; i < num_fms_per_angle; i++) {
    for (int j = 0; j < 4; j++) {
      ofm[j][i] = orientation_map[j * num_fms_per_angle + i];
    }
  }

  // extract conspicuity map for each angle
  saliency_feature_map *nofm[4];
  for (int j = 0; j < 4; j++) {
    nofm[j] = normalized_map(ofm[j], width, height);
    if (nofm[j] == NULL) {
      return NULL;
    }
    nofm[j]->height = height;
    nofm[j]->width = width;
  }

  saliency_feature_map *orientation_cm = aom_malloc(sizeof(*orientation_cm));
  if (!orientation_cm) {
    return NULL;
  }
  orientation_cm->buf =
      (double *)aom_malloc(width * height * sizeof(*orientation_cm->buf));
  if (!orientation_cm->buf) {
    return NULL;
  }
  orientation_cm->height = height;
  orientation_cm->width = width;
  memset(orientation_cm->buf, 0, width * height * sizeof(double));

  for (int i = 0; i < 4; i++) {
    for (int r = 0; r < height; r++) {
      for (int c = 0; c < width; c++) {
        orientation_cm->buf[r * width + c] += nofm[i]->buf[r * width + c];
      }
    }
    aom_free(nofm[i]->buf);
    aom_free(nofm[i]);
  }

  return nomalization_operator(orientation_cm, 8, true);
}

// Set pixel level saliency mask based on Itti-Koch algorithm
int av1_set_saliency_map(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;

  int frm_width = cm->width;
  int frm_height = cm->height;

  double *cr = aom_malloc(frm_width * frm_height * sizeof(*cr));
  double *cg = aom_malloc(frm_width * frm_height * sizeof(*cg));
  double *cb = aom_malloc(frm_width * frm_height * sizeof(*cb));
  double *intensity = aom_malloc(frm_width * frm_height * sizeof(*intensity));

  if (!cr || !cg || !cb || !intensity) {
    aom_free(cr);
    aom_free(cg);
    aom_free(cb);
    aom_free(intensity);
    return 0;
  }

  // Extract red / green / blue channels and intensity component
  get_color_intensity(cpi->source, cm->seq_params->subsampling_x,
                      cm->seq_params->subsampling_y, cr, cg, cb, intensity);

  // Feature Map Extraction
  // intensity map
  saliency_feature_map *i_map[6];
  for (int i = 0; i < 6; i++) {
    i_map[i] = (saliency_feature_map *)aom_malloc(sizeof(*i_map[i]));
    if (!i_map[i]) {
      return 0;
    }
  }

  if (get_feature_map_intensity(intensity, frm_width, frm_height, i_map) == 0) {
    return 0;
  }

  // RGB map
  saliency_feature_map *rg_map[6], *by_map[6];
  for (int i = 0; i < 6; i++) {
    rg_map[i] = (saliency_feature_map *)aom_malloc(sizeof(*rg_map[i]));
    by_map[i] = (saliency_feature_map *)aom_malloc(sizeof(*by_map[i]));
    if (!rg_map[i] || !by_map[i]) {
      aom_free(rg_map[i]);
      aom_free(by_map[i]);
      return 0;
    }
  }

  if (get_feature_map_rgb(cr, cg, cb, frm_width, frm_height, rg_map, by_map) ==
      0) {
    return 0;
  }

  // Orientation map
  saliency_feature_map *orientation_map[24];
  for (int i = 0; i < 24; i++) {
    orientation_map[i] =
        (saliency_feature_map *)aom_malloc(sizeof(*orientation_map[i]));
    if (!orientation_map[i]) {
      return 0;
    }
  }
  if (get_feature_map_orientation(intensity, frm_width, frm_height,
                                  orientation_map) == 0) {
    return 0;
  }

  aom_free(cr);
  aom_free(cg);
  aom_free(cb);
  aom_free(intensity);

  // Conspicuity map generation
  saliency_feature_map *intensity_nm =
      normalized_map(i_map, frm_width, frm_height);
  saliency_feature_map *color_nm =
      normalized_map_rgb(rg_map, by_map, frm_width, frm_height);
  saliency_feature_map *orientation_nm =
      normalized_map_orientation(orientation_map, frm_width, frm_height);

  for (int i = 0; i < 6; i++) {
    aom_free(i_map[i]->buf);
    aom_free(rg_map[i]->buf);
    aom_free(by_map[i]->buf);
    aom_free(i_map[i]);
    aom_free(rg_map[i]);
    aom_free(by_map[i]);
  }

  for (int i = 0; i < 24; i++) {
    aom_free(orientation_map[i]->buf);
    aom_free(orientation_map[i]);
  }

  // Pixel level saliency map
  saliency_feature_map *combined_saliency_map =
      aom_malloc(sizeof(*combined_saliency_map));
  if (!combined_saliency_map) {
    return 0;
  }

  combined_saliency_map->buf = (double *)aom_malloc(
      frm_width * frm_height * sizeof(*combined_saliency_map->buf));
  if (!combined_saliency_map->buf) {
    return 0;
  }
  combined_saliency_map->height = frm_height;
  combined_saliency_map->width = frm_width;

  double w_intensity, w_color, w_orient;

  w_intensity = w_color = w_orient = (double)1 / 3;

  for (int r = 0; r < frm_height; r++) {
    for (int c = 0; c < frm_width; c++) {
      combined_saliency_map->buf[r * frm_width + c] =
          (w_intensity * intensity_nm->buf[r * frm_width + c] +
           w_color * color_nm->buf[r * frm_width + c] +
           w_orient * orientation_nm->buf[r * frm_width + c]);
    }
  }

  combined_saliency_map =
      minmax_normalize(combined_saliency_map);  // Normalization

  for (int r = 0; r < frm_height; r++) {
    for (int c = 0; c < frm_width; c++) {
      int index = r * frm_width + c;
      cpi->saliency_map[index] =
          (uint8_t)(combined_saliency_map->buf[index] * 255);
    }
  }

  aom_free(intensity_nm->buf);
  aom_free(intensity_nm);

  aom_free(color_nm->buf);
  aom_free(color_nm);

  aom_free(orientation_nm->buf);
  aom_free(orientation_nm);

  aom_free(combined_saliency_map->buf);
  aom_free(combined_saliency_map);

  return 1;
}
