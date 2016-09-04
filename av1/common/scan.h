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

#ifndef AV1_COMMON_SCAN_H_
#define AV1_COMMON_SCAN_H_

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NEIGHBORS 2

typedef struct {
  const int16_t *scan;
  const int16_t *iscan;
  const int16_t *neighbors;
} SCAN_ORDER;

extern const SCAN_ORDER av1_default_scan_orders[TX_SIZES];
extern const SCAN_ORDER av1_scan_orders[TX_SIZES][TX_TYPES];

#if CONFIG_ADAPT_SCAN
void update_scan_prob(AV1_COMMON *cm, TX_SIZE tx_size, TX_TYPE tx_type,
                      int rate_16);
void update_scan_count_facade(AV1_COMMON *cm, TX_SIZE tx_size, TX_TYPE tx_type,
                              const tran_low_t *dqcoeffs, int max_scan);

// embed r + c and coeff_idx info with nonzero probabilities. When sorting the
// nonzero probabilities, if there is a tie, the coefficient with smaller r + c
// will be scanned first
void augment_prob(uint32_t *prob, int size, int tx1d_size);

// apply quick sort on nonzero probabilities to obtain a sort order
void update_sort_order(TX_SIZE tx_size, const uint32_t *non_zero_prob,
                       int16_t *sort_order);

// apply topological sort on the nonzero probabilities sorting order to
// guarantee each to-be-scanned coefficient's upper and left coefficient will be
// scanned before the to-be-scanned coefficient.
void update_scan_order(TX_SIZE tx_size, int16_t *sort_order, int16_t *scan,
                       int16_t *iscan);

// For each coeff_idx in scan[], update its above and left neighbors in
// neighbors[] accordingly.
void update_neighbors(int tx_size, const int16_t *scan, const int16_t *iscan,
                      int16_t *neighbors);
void update_scan_order_facade(AV1_COMMON *cm, TX_SIZE tx_size, TX_TYPE tx_type);
void init_scan_order(AV1_COMMON *cm);
#endif

static INLINE int get_coef_context(const int16_t *neighbors,
                                   const uint8_t *token_cache, int c) {
  return (1 + token_cache[neighbors[MAX_NEIGHBORS * c + 0]] +
          token_cache[neighbors[MAX_NEIGHBORS * c + 1]]) >>
         1;
}

static INLINE const SCAN_ORDER *get_scan(TX_SIZE tx_size, TX_TYPE tx_type) {
  return &av1_scan_orders[tx_size][tx_type];
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_SCAN_H_
