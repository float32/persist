// MIT License
//
// Copyright 2023 Tyler Coy
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <algorithm>
#include <type_traits>
#include "inc/crc16.h"

namespace persist
{

enum Result
{
    RESULT_SUCCESS,
    RESULT_FAIL_NO_DATA,
    RESULT_FAIL_ERASE,
    RESULT_FAIL_WRITE,
    RESULT_FAIL_READ,
};

template <typename NVMem, typename TData, uint8_t datatype_version,
    bool assert_fault_tolerant = true>
class Persist
{
public:
    Persist(NVMem& nvmem) : nvmem_{nvmem} {}

    Result Init(void)
    {
        crc_.Init();
        return Reset();
    }

    Result Load(TData& data)
    {
        if (active_block_n_ == -1)
        {
            return RESULT_FAIL_NO_DATA;
        }

        std::memcpy(&data, &block_.data, sizeof(TData));
        return RESULT_SUCCESS;
    }

    Result Save(const TData& data)
    {
        if (DataIsSame(data))
        {
            return RESULT_SUCCESS;
        }

        int32_t next_block = NextWritableBlock(active_block_n_);

        if (next_block == -1)
        {
            if (active_block_n_ == -1)
            {
                if (!nvmem_.Erase(0, kNumPages * kPageSize))
                {
                    return RESULT_FAIL_ERASE;
                }

                next_block = 0;
                sequence_ = 0;
            }
            else
            {
                uint32_t current_page = active_block_n_ / kBlocksPerPage;
                uint32_t next_page = (current_page + 1) % kNumPages;

                if (!nvmem_.Erase(next_page * kPageSize, kPageSize))
                {
                    return RESULT_FAIL_ERASE;
                }

                next_block = next_page * kBlocksPerPage;
                sequence_++;
            }
        }
        else
        {
            sequence_++;
        }

        active_block_n_ = next_block;
        uint32_t location = BlockLocation(active_block_n_);

        std::memcpy(&block_.data, &data, sizeof(TData));
        std::memset(&block_.padding, NVMem::kFillByte, kBlockPaddingSize);
        block_.sequence_n = sequence_;
        block_.crc = GetCRC(block_);

        if (!nvmem_.Write(location, &block_, kBlockSize))
        {
            Reset();
            return RESULT_FAIL_WRITE;
        }

        return RESULT_SUCCESS;
    }

    template <typename First, typename... Rest>
    Result LoadLegacy(TData& data)
    {
        Result result = Load(data);

        if (result == RESULT_FAIL_NO_DATA)
        {
            First l_persist{nvmem_};
            result = l_persist.Init();

            if (result == RESULT_SUCCESS)
            {
                typename First::DataType l_data;
                result = l_persist.template LoadLegacy<Rest...>(l_data);

                if (result == RESULT_SUCCESS)
                {
                    data = static_cast<TData>(l_data);
                }
            }
        }

        return result;
    }

protected:
    static_assert(NVMem::kEraseGranularity <= NVMem::kSize);
    static_assert(NVMem::kWriteGranularity <= NVMem::kSize);
    static_assert(std::is_trivial_v<TData>);
    static_assert(std::is_standard_layout_v<TData>);

    static constexpr
    uint32_t PadSize(uint32_t unpadded_size, uint32_t granularity)
    {
        uint32_t rem = unpadded_size % granularity;
        return (granularity - rem) % granularity;
    }

    using TSequenceNum = uint16_t;
    using TCRC = uint16_t;
    static constexpr uint32_t kBlockPaddingSize = PadSize(
        sizeof(TData) + sizeof(TSequenceNum) + sizeof(TCRC),
        NVMem::kWriteGranularity);

    struct __attribute__ ((packed)) Block
    {
        uint8_t data[sizeof(TData)];
        TSequenceNum sequence_n;
        TCRC crc;
        uint8_t padding[kBlockPaddingSize];
    };

    static constexpr uint32_t kBlockSize = sizeof(Block);
    static constexpr uint32_t kPageSize =
        kBlockSize + PadSize(kBlockSize, NVMem::kEraseGranularity);
    static constexpr uint32_t kBlocksPerPage = kPageSize / kBlockSize;
    static constexpr uint32_t kNumBlocks = std::min<uint32_t>(
        (NVMem::kSize / kPageSize) * kBlocksPerPage,
        std::numeric_limits<TSequenceNum>::max() / 2 + 1);
    static constexpr uint32_t kNumPages =
        (kNumBlocks + kBlocksPerPage - 1) / kBlocksPerPage;

    static_assert(kBlocksPerPage > 0);
    static_assert(kNumPages > 0);
    static_assert(kNumBlocks > 0);
    static_assert(assert_fault_tolerant == false || kNumPages >= 2,
        "Region is not fault-tolerant");

    NVMem& nvmem_;
    Block block_;
    int32_t active_block_n_;
    TSequenceNum sequence_;
    Crc16 crc_;

    Result Reset(void)
    {
        sequence_ = 0;
        active_block_n_ = -1;

        for (uint32_t i = 0; i < kNumBlocks; i++)
        {
            uint32_t location = BlockLocation(i);

            if (!nvmem_.Read(&block_, location, kBlockSize))
            {
                active_block_n_ = -1;
                return RESULT_FAIL_READ;
            }

            if (block_.crc == GetCRC(block_))
            {
                TSequenceNum sn = block_.sequence_n;
                constexpr int32_t num_blocks = kNumBlocks;

                if ((active_block_n_ == -1) ||
                    ((sn > sequence_) && (sn - sequence_ < num_blocks)) ||
                    ((sn < sequence_) && (sequence_ - sn >= num_blocks)))
                {
                    active_block_n_ = i;
                    sequence_ = sn;
                }
            }
        }

        if (active_block_n_ != -1)
        {
            uint32_t location = BlockLocation(active_block_n_);

            if (!nvmem_.Read(&block_, location, kBlockSize))
            {
                active_block_n_ = -1;
                return RESULT_FAIL_READ;
            }
        }

        return RESULT_SUCCESS;
    }

    TCRC GetCRC(const Block& block)
    {
        TCRC seed = datatype_version;
        crc_.Seed(seed | (~seed << 8));
        return crc_.Process(&block, sizeof(TData) + sizeof(TSequenceNum));
    }

    uint32_t BlockLocation(uint32_t block_n)
    {
        uint32_t page_n = block_n / kBlocksPerPage;
        block_n -= page_n * kBlocksPerPage;
        return page_n * kPageSize + block_n * kBlockSize;
    }

    int32_t NextWritableBlock(int32_t current_block_n)
    {
        if (current_block_n == -1)
        {
            current_block_n = kNumBlocks - 1;
        }

        int32_t next_block_n = current_block_n;

        do
        {
            next_block_n = (next_block_n + 1) % kNumBlocks;

            if (nvmem_.Writable(BlockLocation(next_block_n), kBlockSize))
            {
                break;
            }
        }
        while (next_block_n != current_block_n);

        return (next_block_n == current_block_n) ? -1 : next_block_n;
    }

    bool DataIsSame(const TData& data)
    {
        return (active_block_n_ != -1) &&
            (0 == std::memcmp(&block_.data, &data, sizeof(TData)));
    }

    template <typename A, typename B, uint8_t C, bool D> friend class Persist;
    using DataType = TData;

    template <typename... Ts, std::enable_if_t<sizeof...(Ts) == 0, bool> = true>
    Result LoadLegacy(TData& data)
    {
        return Load(data);
    }
};

}
