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

namespace persist
{

class Crc16
{
public:
    void Init(void)
    {
        if (!initialized_)
        {
            for (uint32_t i = 0; i < 16; i++)
            {
                ltable_[i] = ComputeTableEntry(i);
                htable_[i] = ComputeTableEntry(i << 4);
            }

            initialized_ = true;
        }

        crc_ = 0;
    }

    void Seed(uint16_t crc)
    {
        crc_ = crc;
    }

    uint16_t Process(const void* data, uint32_t length)
    {
        auto byte = reinterpret_cast<const uint8_t*>(data);

        while (length--)
        {
            uint8_t index = (crc_ >> 8) ^ *(byte++);
            crc_ = (crc_ << 8) ^ ltable_[index & 0xF] ^ htable_[index >> 4];
        }

        return crc();
    }

    uint16_t crc(void) const
    {
        return crc_;
    }

protected:
    static constexpr uint16_t kPolynomial = 0x1021;

    static inline bool initialized_;
    static inline uint16_t ltable_[16];
    static inline uint16_t htable_[16];
    uint16_t crc_;

    static constexpr uint16_t ComputeTableEntry(uint16_t x)
    {
        x <<= 8;

        for (uint32_t j = 0; j < 8; j++)
        {
            if (x & 0x8000)
            {
                x <<= 1;
                x ^= kPolynomial;
            }
            else
            {
                x <<= 1;
            }
        }

        return x;
    }
};

}
