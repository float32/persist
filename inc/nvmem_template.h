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

// This is a template interface for the NVMem type used by Persist which should
// be adapted to the non-volatile memory used by our application. Note that
// Persist assumes that NVMem is already initialized.
struct NVMemTemplate
{
    // Total size in bytes of the NVMem region.
    static constexpr uint32_t kSize = 0;

    // Size of the smallest chunk of NVMem that may be erased at once. For flash
    // this is often something like 1kB, while for EEPROM it may be a single
    // byte. Must not be larger than kSize.
    static constexpr uint32_t kEraseGranularity = 0;

    // Size of the smallest chunk of NVMem that may be written at once. Must not
    // be larger than kSize.
    static constexpr uint32_t kWriteGranularity = 0;

    // Persist will use this value to fill any padding.
    static constexpr uint8_t kFillByte = 0;

    // For each of these functions, `location` is an offset in bytes from the
    // beginning of the NVMem region.

    // Copy `size` bytes from `location` to `dst`. Return `true` on success or
    // `false` on failure.
    bool Read(void* dst, uint32_t location, uint32_t size);

    // Determine if `size` bytes starting at `location` are immediately writable
    // without requiring erasure first.
    bool Writable(uint32_t location, uint32_t size);

    // Copy `size` bytes from `src` and write at `location`.  Return `true` on
    // success or `false` on failure.
    bool Write(uint32_t location, const void* src, uint32_t size);

    // Erase `size` bytes starting at `location`.  Return `true` on success or
    // `false` on failure.
    bool Erase(uint32_t location, uint32_t size);
};
