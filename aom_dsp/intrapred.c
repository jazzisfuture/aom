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
#include "./aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"

#define DST(x, y) dst[(x) + (y)*stride]
#define AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

static INLINE void d207_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  (void)above;
  // first column
  for (r = 0; r < bs - 1; ++r) dst[r * stride] = AVG2(left[r], left[r + 1]);
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

  // second column
  for (r = 0; r < bs - 2; ++r)
    dst[r * stride] = AVG3(left[r], left[r + 1], left[r + 2]);
  dst[(bs - 2) * stride] = AVG3(left[bs - 2], left[bs - 1], left[bs - 1]);
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

  // rest of last row
  for (c = 0; c < bs - 2; ++c) dst[(bs - 1) * stride + c] = left[bs - 1];

  for (r = bs - 2; r >= 0; --r)
    for (c = 0; c < bs - 2; ++c)
      dst[r * stride + c] = dst[(r + 1) * stride + c - 2];
}

#if CONFIG_MISC_FIXES
static INLINE void d207e_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                   const uint8_t *above, const uint8_t *left) {
  int r, c;
  (void)above;

  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = c & 1 ? AVG3(left[(c >> 1) + r], left[(c >> 1) + r + 1],
                            left[(c >> 1) + r + 2])
                     : AVG2(left[(c >> 1) + r], left[(c >> 1) + r + 1]);
    }
    dst += stride;
  }
}
#endif  // CONFIG_MISC_FIXES

static INLINE void d63_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                 const uint8_t *above, const uint8_t *left) {
  int r, c;
  int size;
  (void)left;
  for (c = 0; c < bs; ++c) {
    dst[c] = AVG2(above[c], above[c + 1]);
    dst[stride + c] = AVG3(above[c], above[c + 1], above[c + 2]);
  }
  for (r = 2, size = bs - 2; r < bs; r += 2, --size) {
    memcpy(dst + (r + 0) * stride, dst + (r >> 1), size);
    memset(dst + (r + 0) * stride + size, above[bs - 1], bs - size);
    memcpy(dst + (r + 1) * stride, dst + stride + (r >> 1), size);
    memset(dst + (r + 1) * stride + size, above[bs - 1], bs - size);
  }
}

#if CONFIG_MISC_FIXES
static INLINE void d63e_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  (void)left;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = r & 1 ? AVG3(above[(r >> 1) + c], above[(r >> 1) + c + 1],
                            above[(r >> 1) + c + 2])
                     : AVG2(above[(r >> 1) + c], above[(r >> 1) + c + 1]);
    }
    dst += stride;
  }
}
#endif  // CONFIG_MISC_FIXES

static INLINE void d45_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                 const uint8_t *above, const uint8_t *left) {
  const uint8_t above_right = above[bs - 1];
  const uint8_t *const dst_row0 = dst;
  int x, size;
  (void)left;

  for (x = 0; x < bs - 1; ++x) {
    dst[x] = AVG3(above[x], above[x + 1], above[x + 2]);
  }
  dst[bs - 1] = above_right;
  dst += stride;
  for (x = 1, size = bs - 2; x < bs; ++x, --size) {
    memcpy(dst, dst_row0 + x, size);
    memset(dst + size, above_right, x + 1);
    dst += stride;
  }
}

#if CONFIG_MISC_FIXES
static INLINE void d45e_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  (void)left;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = AVG3(above[r + c], above[r + c + 1],
                    above[r + c + 1 + (r + c + 2 < bs * 2)]);
    }
    dst += stride;
  }
}
#endif  // CONFIG_MISC_FIXES

static INLINE void d117_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;

  // first row
  for (c = 0; c < bs; c++) dst[c] = AVG2(above[c - 1], above[c]);
  dst += stride;

  // second row
  dst[0] = AVG3(left[0], above[-1], above[0]);
  for (c = 1; c < bs; c++) dst[c] = AVG3(above[c - 2], above[c - 1], above[c]);
  dst += stride;

  // the rest of first col
  dst[0] = AVG3(above[-1], left[0], left[1]);
  for (r = 3; r < bs; ++r)
    dst[(r - 2) * stride] = AVG3(left[r - 3], left[r - 2], left[r - 1]);

  // the rest of the block
  for (r = 2; r < bs; ++r) {
    for (c = 1; c < bs; c++) dst[c] = dst[-2 * stride + c - 1];
    dst += stride;
  }
}

static INLINE void d135_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  dst[0] = AVG3(left[0], above[-1], above[0]);
  for (c = 1; c < bs; c++) dst[c] = AVG3(above[c - 2], above[c - 1], above[c]);

  dst[stride] = AVG3(above[-1], left[0], left[1]);
  for (r = 2; r < bs; ++r)
    dst[r * stride] = AVG3(left[r - 2], left[r - 1], left[r]);

  dst += stride;
  for (r = 1; r < bs; ++r) {
    for (c = 1; c < bs; c++) dst[c] = dst[-stride + c - 1];
    dst += stride;
  }
}

static INLINE void d153_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  dst[0] = AVG2(above[-1], left[0]);
  for (r = 1; r < bs; r++) dst[r * stride] = AVG2(left[r - 1], left[r]);
  dst++;

  dst[0] = AVG3(left[0], above[-1], above[0]);
  dst[stride] = AVG3(above[-1], left[0], left[1]);
  for (r = 2; r < bs; r++)
    dst[r * stride] = AVG3(left[r - 2], left[r - 1], left[r]);
  dst++;

  for (c = 0; c < bs - 2; c++)
    dst[c] = AVG3(above[c - 1], above[c], above[c + 1]);
  dst += stride;

  for (r = 1; r < bs; ++r) {
    for (c = 0; c < bs - 2; c++) dst[c] = dst[-stride + c - 2];
    dst += stride;
  }
}

static INLINE void v_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                               const uint8_t *above, const uint8_t *left) {
  int r;
  (void)left;

  for (r = 0; r < bs; r++) {
    memcpy(dst, above, bs);
    dst += stride;
  }
}

static INLINE void h_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                               const uint8_t *above, const uint8_t *left) {
  int r;
  (void)above;

  for (r = 0; r < bs; r++) {
    memset(dst, left[r], bs);
    dst += stride;
  }
}

static INLINE void tm_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                const uint8_t *above, const uint8_t *left) {
  int r, c;
  int ytop_left = above[-1];

  for (r = 0; r < bs; r++) {
    for (c = 0; c < bs; c++)
      dst[c] = clip_pixel(left[r] + above[c] - ytop_left);
    dst += stride;
  }
}

static INLINE void dc_128_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                    const uint8_t *above, const uint8_t *left) {
  int r;
  (void)above;
  (void)left;

  for (r = 0; r < bs; r++) {
    memset(dst, 128, bs);
    dst += stride;
  }
}

static INLINE void dc_left_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  (void)above;

  for (i = 0; i < bs; i++) sum += left[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    memset(dst, expected_dc, bs);
    dst += stride;
  }
}

static INLINE void dc_top_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                    const uint8_t *above, const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  (void)left;

  for (i = 0; i < bs; i++) sum += above[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    memset(dst, expected_dc, bs);
    dst += stride;
  }
}

static INLINE void dc_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                const uint8_t *above, const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  const int count = 2 * bs;

  for (i = 0; i < bs; i++) {
    sum += above[i];
    sum += left[i];
  }

  expected_dc = (sum + (count >> 1)) / count;

  for (r = 0; r < bs; r++) {
    memset(dst, expected_dc, bs);
    dst += stride;
  }
}

void aom_he_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *above, const uint8_t *left) {
  const int H = above[-1];
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];

  memset(dst + stride * 0, AVG3(H, I, J), 4);
  memset(dst + stride * 1, AVG3(I, J, K), 4);
  memset(dst + stride * 2, AVG3(J, K, L), 4);
  memset(dst + stride * 3, AVG3(K, L, L), 4);
}

void aom_ve_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *above, const uint8_t *left) {
  const int H = above[-1];
  const int I = above[0];
  const int J = above[1];
  const int K = above[2];
  const int L = above[3];
  const int M = above[4];

  dst[0] = AVG3(H, I, J);
  dst[1] = AVG3(I, J, K);
  dst[2] = AVG3(J, K, L);
  dst[3] = AVG3(K, L, M);
  memcpy(dst + stride * 1, dst, 4);
  memcpy(dst + stride * 2, dst, 4);
  memcpy(dst + stride * 3, dst, 4);
}

void aom_d207_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  (void)above;
  DST(0, 0) = AVG2(I, J);
  DST(2, 0) = DST(0, 1) = AVG2(J, K);
  DST(2, 1) = DST(0, 2) = AVG2(K, L);
  DST(1, 0) = AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) = DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

void aom_d63_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  (void)left;
  DST(0, 0) = AVG2(A, B);
  DST(1, 0) = DST(0, 2) = AVG2(B, C);
  DST(2, 0) = DST(1, 2) = AVG2(C, D);
  DST(3, 0) = DST(2, 2) = AVG2(D, E);
  DST(3, 2) = AVG2(E, F);  // differs from aom

  DST(0, 1) = AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
  DST(3, 3) = AVG3(E, F, G);  // differs from aom
}

void aom_d63f_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  const int H = above[7];
  (void)left;
  DST(0, 0) = AVG2(A, B);
  DST(1, 0) = DST(0, 2) = AVG2(B, C);
  DST(2, 0) = DST(1, 2) = AVG2(C, D);
  DST(3, 0) = DST(2, 2) = AVG2(D, E);
  DST(3, 2) = AVG3(E, F, G);

  DST(0, 1) = AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
  DST(3, 3) = AVG3(F, G, H);
}

void aom_d45_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  const int H = above[7];
  (void)stride;
  (void)left;
  DST(0, 0) = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2) = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
  DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
  DST(3, 3) = H;  // differs from aom
}

void aom_d45e_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  const int H = above[7];
  (void)stride;
  (void)left;
  DST(0, 0) = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2) = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
  DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
  DST(3, 3) = AVG3(G, H, H);
}

void aom_d117_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  DST(0, 0) = DST(1, 2) = AVG2(X, A);
  DST(1, 0) = DST(2, 2) = AVG2(A, B);
  DST(2, 0) = DST(3, 2) = AVG2(B, C);
  DST(3, 0) = AVG2(C, D);

  DST(0, 3) = AVG3(K, J, I);
  DST(0, 2) = AVG3(J, I, X);
  DST(0, 1) = DST(1, 3) = AVG3(I, X, A);
  DST(1, 1) = DST(2, 3) = AVG3(X, A, B);
  DST(2, 1) = DST(3, 3) = AVG3(A, B, C);
  DST(3, 1) = AVG3(B, C, D);
}

void aom_d135_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  (void)stride;
  DST(0, 3) = AVG3(J, K, L);
  DST(1, 3) = DST(0, 2) = AVG3(I, J, K);
  DST(2, 3) = DST(1, 2) = DST(0, 1) = AVG3(X, I, J);
  DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = AVG3(A, X, I);
  DST(3, 2) = DST(2, 1) = DST(1, 0) = AVG3(B, A, X);
  DST(3, 1) = DST(2, 0) = AVG3(C, B, A);
  DST(3, 0) = AVG3(D, C, B);
}

void aom_d153_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];

  DST(0, 0) = DST(2, 1) = AVG2(I, X);
  DST(0, 1) = DST(2, 2) = AVG2(J, I);
  DST(0, 2) = DST(2, 3) = AVG2(K, J);
  DST(0, 3) = AVG2(L, K);

  DST(3, 0) = AVG3(A, B, C);
  DST(2, 0) = AVG3(X, A, B);
  DST(1, 0) = DST(3, 1) = AVG3(I, X, A);
  DST(1, 1) = DST(3, 2) = AVG3(J, I, X);
  DST(1, 2) = DST(3, 3) = AVG3(K, J, I);
  DST(1, 3) = AVG3(L, K, J);
}

#if CONFIG_AOM_HIGHBITDEPTH
static INLINE void highbd_d207_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)above;
  (void)bd;

  // First column.
  for (r = 0; r < bs - 1; ++r) {
    dst[r * stride] = AVG2(left[r], left[r + 1]);
  }
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

  // Second column.
  for (r = 0; r < bs - 2; ++r) {
    dst[r * stride] = AVG3(left[r], left[r + 1], left[r + 2]);
  }
  dst[(bs - 2) * stride] = AVG3(left[bs - 2], left[bs - 1], left[bs - 1]);
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

  // Rest of last row.
  for (c = 0; c < bs - 2; ++c) dst[(bs - 1) * stride + c] = left[bs - 1];

  for (r = bs - 2; r >= 0; --r) {
    for (c = 0; c < bs - 2; ++c)
      dst[r * stride + c] = dst[(r + 1) * stride + c - 2];
  }
}

#if CONFIG_MISC_FIXES
static INLINE void highbd_d207e_predictor(uint16_t *dst, ptrdiff_t stride,
                                          int bs, const uint16_t *above,
                                          const uint16_t *left, int bd) {
  int r, c;
  (void)above;
  (void)bd;

  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = c & 1 ? AVG3(left[(c >> 1) + r], left[(c >> 1) + r + 1],
                            left[(c >> 1) + r + 2])
                     : AVG2(left[(c >> 1) + r], left[(c >> 1) + r + 1]);
    }
    dst += stride;
  }
}
#endif  // CONFIG_MISC_FIXES

static INLINE void highbd_d63_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  int r, c;
  (void)left;
  (void)bd;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = r & 1 ? AVG3(above[(r >> 1) + c], above[(r >> 1) + c + 1],
                            above[(r >> 1) + c + 2])
                     : AVG2(above[(r >> 1) + c], above[(r >> 1) + c + 1]);
    }
    dst += stride;
  }
}

#define highbd_d63e_predictor highbd_d63_predictor

static INLINE void highbd_d45_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  int r, c;
  (void)left;
  (void)bd;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = r + c + 2 < bs * 2
                   ? AVG3(above[r + c], above[r + c + 1], above[r + c + 2])
                   : above[bs * 2 - 1];
    }
    dst += stride;
  }
}

#if CONFIG_MISC_FIXES
static INLINE void highbd_d45e_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)left;
  (void)bd;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = AVG3(above[r + c], above[r + c + 1],
                    above[r + c + 1 + (r + c + 2 < bs * 2)]);
    }
    dst += stride;
  }
}
#endif  // CONFIG_MISC_FIXES

static INLINE void highbd_d117_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)bd;

  // first row
  for (c = 0; c < bs; c++) dst[c] = AVG2(above[c - 1], above[c]);
  dst += stride;

  // second row
  dst[0] = AVG3(left[0], above[-1], above[0]);
  for (c = 1; c < bs; c++) dst[c] = AVG3(above[c - 2], above[c - 1], above[c]);
  dst += stride;

  // the rest of first col
  dst[0] = AVG3(above[-1], left[0], left[1]);
  for (r = 3; r < bs; ++r)
    dst[(r - 2) * stride] = AVG3(left[r - 3], left[r - 2], left[r - 1]);

  // the rest of the block
  for (r = 2; r < bs; ++r) {
    for (c = 1; c < bs; c++) dst[c] = dst[-2 * stride + c - 1];
    dst += stride;
  }
}

static INLINE void highbd_d135_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)bd;
  dst[0] = AVG3(left[0], above[-1], above[0]);
  for (c = 1; c < bs; c++) dst[c] = AVG3(above[c - 2], above[c - 1], above[c]);

  dst[stride] = AVG3(above[-1], left[0], left[1]);
  for (r = 2; r < bs; ++r)
    dst[r * stride] = AVG3(left[r - 2], left[r - 1], left[r]);

  dst += stride;
  for (r = 1; r < bs; ++r) {
    for (c = 1; c < bs; c++) dst[c] = dst[-stride + c - 1];
    dst += stride;
  }
}

static INLINE void highbd_d153_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)bd;
  dst[0] = AVG2(above[-1], left[0]);
  for (r = 1; r < bs; r++) dst[r * stride] = AVG2(left[r - 1], left[r]);
  dst++;

  dst[0] = AVG3(left[0], above[-1], above[0]);
  dst[stride] = AVG3(above[-1], left[0], left[1]);
  for (r = 2; r < bs; r++)
    dst[r * stride] = AVG3(left[r - 2], left[r - 1], left[r]);
  dst++;

  for (c = 0; c < bs - 2; c++)
    dst[c] = AVG3(above[c - 1], above[c], above[c + 1]);
  dst += stride;

  for (r = 1; r < bs; ++r) {
    for (c = 0; c < bs - 2; c++) dst[c] = dst[-stride + c - 2];
    dst += stride;
  }
}

static INLINE void highbd_v_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  int r;
  (void)left;
  (void)bd;
  for (r = 0; r < bs; r++) {
    memcpy(dst, above, bs * sizeof(uint16_t));
    dst += stride;
  }
}

static INLINE void highbd_h_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  int r;
  (void)above;
  (void)bd;
  for (r = 0; r < bs; r++) {
    aom_memset16(dst, left[r], bs);
    dst += stride;
  }
}

static INLINE void highbd_tm_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int r, c;
  int ytop_left = above[-1];
  (void)bd;

  for (r = 0; r < bs; r++) {
    for (c = 0; c < bs; c++)
      dst[c] = clip_pixel_highbd(left[r] + above[c] - ytop_left, bd);
    dst += stride;
  }
}

static INLINE void highbd_dc_128_predictor(uint16_t *dst, ptrdiff_t stride,
                                           int bs, const uint16_t *above,
                                           const uint16_t *left, int bd) {
  int r;
  (void)above;
  (void)left;

  for (r = 0; r < bs; r++) {
    aom_memset16(dst, 128 << (bd - 8), bs);
    dst += stride;
  }
}

static INLINE void highbd_dc_left_predictor(uint16_t *dst, ptrdiff_t stride,
                                            int bs, const uint16_t *above,
                                            const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  (void)above;
  (void)bd;

  for (i = 0; i < bs; i++) sum += left[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    aom_memset16(dst, expected_dc, bs);
    dst += stride;
  }
}

static INLINE void highbd_dc_top_predictor(uint16_t *dst, ptrdiff_t stride,
                                           int bs, const uint16_t *above,
                                           const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  (void)left;
  (void)bd;

  for (i = 0; i < bs; i++) sum += above[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    aom_memset16(dst, expected_dc, bs);
    dst += stride;
  }
}

static INLINE void highbd_dc_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  const int count = 2 * bs;
  (void)bd;

  for (i = 0; i < bs; i++) {
    sum += above[i];
    sum += left[i];
  }

  expected_dc = (sum + (count >> 1)) / count;

  for (r = 0; r < bs; r++) {
    aom_memset16(dst, expected_dc, bs);
    dst += stride;
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

// This serves as a wrapper function, so that all the prediction functions
// can be unified and accessed as a pointer array. Note that the boundary
// above and left are not necessarily used all the time.
#define intra_pred_sized(type, size)                        \
  void aom_##type##_predictor_##size##x##size##_c(          \
      uint8_t *dst, ptrdiff_t stride, const uint8_t *above, \
      const uint8_t *left) {                                \
    type##_predictor(dst, stride, size, above, left);       \
  }

#if CONFIG_AOM_HIGHBITDEPTH
#define intra_pred_highbd_sized(type, size)                        \
  void aom_highbd_##type##_predictor_##size##x##size##_c(          \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,      \
      const uint16_t *left, int bd) {                              \
    highbd_##type##_predictor(dst, stride, size, above, left, bd); \
  }

#define intra_pred_allsizes(type)                                           \
  intra_pred_sized(type, 4) intra_pred_sized(type, 8)                       \
      intra_pred_sized(type, 16) intra_pred_sized(type, 32)                 \
          intra_pred_highbd_sized(type, 4) intra_pred_highbd_sized(type, 8) \
              intra_pred_highbd_sized(type, 16)                             \
                  intra_pred_highbd_sized(type, 32)

#define intra_pred_no_4x4(type)                                              \
  intra_pred_sized(type, 8) intra_pred_sized(type, 16)                       \
      intra_pred_sized(type, 32) intra_pred_highbd_sized(type, 4)            \
          intra_pred_highbd_sized(type, 8) intra_pred_highbd_sized(type, 16) \
              intra_pred_highbd_sized(type, 32)

#else
#define intra_pred_allsizes(type)                                       \
  intra_pred_sized(type, 2) intra_pred_sized(type, 4) intra_pred_sized( \
      type, 8) intra_pred_sized(type, 16) intra_pred_sized(type, 32)

#define intra_pred_above_4x4(type)                     \
  intra_pred_sized(type, 8) intra_pred_sized(type, 16) \
      intra_pred_sized(type, 32)
#endif  // CONFIG_AOM_HIGHBITDEPTH

/* clang-format off */
intra_pred_above_4x4(d207)
intra_pred_above_4x4(d63)
intra_pred_above_4x4(d45)
#if CONFIG_MISC_FIXES
intra_pred_allsizes(d207e)
intra_pred_allsizes(d63e)
intra_pred_above_4x4(d45e)
#endif
intra_pred_above_4x4(d117)
intra_pred_above_4x4(d135)
intra_pred_above_4x4(d153)
intra_pred_allsizes(v)
intra_pred_allsizes(h)
intra_pred_allsizes(tm)
intra_pred_allsizes(dc_128)
intra_pred_allsizes(dc_left)
intra_pred_allsizes(dc_top)
intra_pred_allsizes(dc)
#undef intra_pred_allsizes
    /* clang-format on */
