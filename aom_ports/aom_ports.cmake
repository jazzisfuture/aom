##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
set(AOM_PORTS_INCLUDE_ONLY_SOURCES
    "${AOM_ROOT}/aom_ports/aom_once.h"
    "${AOM_ROOT}/aom_ports/aom_timer.h"
    "${AOM_ROOT}/aom_ports/bitops.h"
    "${AOM_ROOT}/aom_ports/emmintrin_compat.h"
    "${AOM_ROOT}/aom_ports/mem.h"
    "${AOM_ROOT}/aom_ports/mem_ops.h"
    "${AOM_ROOT}/aom_ports/mem_ops_aligned.h"
    "${AOM_ROOT}/aom_ports/msvc.h"
    "${AOM_ROOT}/aom_ports/system_state.h")

set(AOM_PORTS_INCLUDE_ONLY_SOURCES_X86
    "${AOM_ROOT}/aom_ports/x86_abi_support.asm")

set(AOM_PORTS_ASM_MMX "${AOM_ROOT}/aom_ports/emms.asm")

# For targets where HAVE_MMX is true:
#   Creates the aom_ports build target, adds the includes in aom_ports to the
#   target, and makes libaom depend on it.
# Otherwise:
#   Adds the includes in aom_ports to the libaom target.
# For all target platforms:
#   The libaom target must exist before this function is called.
function (setup_aom_ports_targets)
  if (HAVE_MMX)
    add_asm_library("aom_ports" "AOM_PORTS_ASM_MMX" "aom")
    set(aom_ports_has_symbols 1)
  endif ()

  if (aom_ports_has_symbols)
    target_sources(aom_ports PUBLIC ${AOM_PORTS_INCLUDE_ONLY_SOURCES})

    if ("${AOM_TARGET_CPU}" STREQUAL "x86" OR
        "${AOM_TARGET_CPU}" STREQUAL "x86_64")
      target_sources(aom_ports PUBLIC ${AOM_PORTS_INCLUDE_ONLY_SOURCES_X86})
    endif ()
  else ()
    target_sources(aom PUBLIC ${AOM_PORTS_INCLUDE_ONLY_SOURCES})

    if ("${AOM_TARGET_CPU}" STREQUAL "x86" OR
        "${AOM_TARGET_CPU}" STREQUAL "x86_64")
      target_sources(aom PUBLIC ${AOM_PORTS_INCLUDE_ONLY_SOURCES_X86})
    endif ()
  endif ()
endfunction ()
