/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_DSP_ENTENC_H_
#define AOM_AOM_DSP_ENTENC_H_
#include <stddef.h>
#include "aom_dsp/entcode.h"
#include "aom_ports/msvc.h"

/* Needed for MSVC */
#ifdef _MSC_VER
#if defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#define USE_MSC_INTRINSICS
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t enc_od_ec_window;

typedef struct od_ec_enc od_ec_enc;

#define OD_MEASURE_EC_OVERHEAD (0)

/*The entropy encoder context.*/
struct od_ec_enc {
  /*Buffered output.
    This contains only the raw bits until the final call to od_ec_enc_done(),
     where all the arithmetic-coded data gets prepended to it.*/
  unsigned char *buf;
  /*The size of the buffer.*/
  uint32_t storage;
  /*The offset at which the next entropy-coded byte will be written.*/
  uint32_t offs;
  /*The low end of the current range.*/
  enc_od_ec_window low;
  /*The number of values in the current range.*/
  uint16_t rng;
  /*The number of bits of data in the current value.*/
  int16_t cnt;
  /*Nonzero if an error occurred.*/
  int error;
#if OD_MEASURE_EC_OVERHEAD
  double entropy;
  int nb_symbols;
#endif
};

/*See entenc.c for further documentation.*/

void od_ec_enc_init(od_ec_enc *enc, uint32_t size) OD_ARG_NONNULL(1);
void od_ec_enc_reset(od_ec_enc *enc) OD_ARG_NONNULL(1);
void od_ec_enc_clear(od_ec_enc *enc) OD_ARG_NONNULL(1);

void od_ec_encode_bool_q15(od_ec_enc *enc, int val, unsigned f_q15)
    OD_ARG_NONNULL(1);
void od_ec_encode_cdf_q15(od_ec_enc *enc, int s, const uint16_t *cdf, int nsyms)
    OD_ARG_NONNULL(1) OD_ARG_NONNULL(3);

void od_ec_enc_bits(od_ec_enc *enc, uint32_t fl, unsigned ftb)
    OD_ARG_NONNULL(1);

void od_ec_enc_patch_initial_bits(od_ec_enc *enc, unsigned val, int nbits)
    OD_ARG_NONNULL(1);
OD_WARN_UNUSED_RESULT unsigned char *od_ec_enc_done(od_ec_enc *enc,
                                                    uint32_t *nbytes)
    OD_ARG_NONNULL(1) OD_ARG_NONNULL(2);

OD_WARN_UNUSED_RESULT int od_ec_enc_tell(const od_ec_enc *enc)
    OD_ARG_NONNULL(1);
OD_WARN_UNUSED_RESULT uint32_t od_ec_enc_tell_frac(const od_ec_enc *enc)
    OD_ARG_NONNULL(1);

void od_ec_enc_checkpoint(od_ec_enc *dst, const od_ec_enc *src);
void od_ec_enc_rollback(od_ec_enc *dst, const od_ec_enc *src);

// buf is the frame bitbuffer, offs is where carry to be added
static AOM_INLINE void propagate_carry_bwd(unsigned char *buf, uint32_t offs) {
  uint16_t sum, carry = 1;
  while (carry && (offs > 0)) {
    sum = (uint16_t)buf[offs] + 1;
    buf[offs--] = (unsigned char)sum;
    carry = sum >> 8;
  }
  if (carry) {
    sum = (uint16_t)buf[offs] + 1;
    buf[offs] = (unsigned char)sum;
    assert(!(sum >> 8));
  }
}

static AOM_INLINE void write_enc_data_to_out_buf(unsigned char *out,
                                                 uint32_t offs, uint64_t output,
                                                 uint64_t carry,
                                                 uint32_t *enc_offs,
                                                 uint8_t num_bytes_ready) {
#if defined(__GNUC__)
  // GNU builtin
  const uint64_t reg =
      __builtin_bswap64(output) >> ((8 - num_bytes_ready) << 3);
  memcpy(&out[offs], &reg, 8);
  // Propagate carry backwards if exists
  if (carry) {
    assert(offs > 0);
    propagate_carry_bwd(out, offs - 1);
  }
  *enc_offs = offs + num_bytes_ready;
#elif defined(USE_MSC_INTRINSICS)
  // MSVC
  const uint64_t reg = _byteswap_uint64(output) >> ((8 - num_bytes_ready) << 3);
  memcpy(&out[offs], &reg, 8);
  // Propagate carry backwards if exists
  if (carry) {
    assert(offs > 0);
    propagate_carry_bwd(out, offs - 1);
  }
  *enc_offs = offs + num_bytes_ready;
#else
  // C function
  int num_bytes_ready_minus_1 = num_bytes_ready - 1;
  assert(num_bytes_ready_minus_1 >= 0);
  uint16_t val = (uint16_t)(output >> (8 * num_bytes_ready_minus_1--));
  out[offs] = (unsigned char)(val & 0x00FF);
  if (carry) propagate_carry_bwd(out, offs - 1);
  offs++;
  while (num_bytes_ready_minus_1 >= 0) {
    val = (uint16_t)(output >> (8 * (num_bytes_ready_minus_1--)));
    out[offs++] = (unsigned char)(val & 0x00FF);
  }
  *enc_offs = offs;
#endif
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_DSP_ENTENC_H_
