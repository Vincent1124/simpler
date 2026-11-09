/*
 * Copyright (c) PyPTO Contributors.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 * -----------------------------------------------------------------------------------------------------------
 */

#pragma once

#include <cstdint>
#include <pto/pto-inst.hpp>

#include "intrinsic.h"
#include "tensor.h"

#ifndef __gm__
#define __gm__
#endif
#ifndef __aicore__
#define __aicore__ [aicore]  // NOLINT(whitespace/braces)
#endif

inline __aicore__ void stamp_block(__gm__ int64_t *args, int64_t lane) {
    auto *tensor = reinterpret_cast<__gm__ Tensor *>(args[0]);
    auto *out = reinterpret_cast<__gm__ int64_t *>(tensor->buffer.addr) + tensor->start_offset;
    const int64_t index = args[1] + get_block_idx(args);
    const int64_t offset = index * 48 + lane * 16;
    out[offset] = index + 1;
    out[offset + 1] = get_block_num(args);
    out[offset + 2] = lane;
    dcci(&out[offset], cache_line_t::SINGLE_CACHE_LINE, dcci_dst_t::CACHELINE_OUT);
    dsb((mem_dsb_t)0);
}
