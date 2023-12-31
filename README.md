# persist

Fault-tolerant persistent data storage for embedded systems

https://github.com/float32/persist

---

## Features

- **Robust**: Prevents data loss in the event of a fault while saving data.
    Data integrity is verified by CRC-16. Memory is wear-leveled.
- **Portable**: No assumptions made about underlying hardware. No dependencies
    outside of the C++ standard library.
- **Header only** for your convenience.
- **MIT license**


## Limitations

- Requires a C++17 compiler.
- Vulnerable to failure due to memory wear or bad blocks.


## Theory of operation

Any contiguous region of memory has the following attributes:

- Erase granularity: The size of the smallest chunk of memory that may
  be erased at once.
- Write granularity: The size of the smallest chunk of memory that may
  be written at once.

Given a data structure of interest named Data, we define a structure named Block
containing one Data and some bookkeeping information. The size of Block is
padded to a multiple of the write granularity.

We define a Page as a contiguous region of memory with the smallest possible
size which:

- Is a multiple of the erase granularity.
- Can contain at least one Block.

A Block may never span across Pages. So, a region of memory might be
conceptually divided up like so:

```
Location    0       8       16      24      32      40      48      56      64
Erase gran. |---------------|---------------|---------------|---------------|
Write gran. |-------|-------|-------|-------|-------|-------|-------|-------|
Page        |-------------------------------|-------------------------------|
Block       |-----------------------|       |-----------------------|
```

In addition to its Data, each Block contains a 16-bit Sequence Number (SN) and
a CRC-16 which verifies the integrity of both the Data and the SN. The first
Block written is assigned an SN of 0 and for each subsequent Block, the SN is
incremented by 1 (mod 2<sup>16</sup>).

To load saved Data, we first scan the memory region for valid Blocks (i.e.
Blocks with a matching CRC), keeping track of the most recently written Block by
comparing their SNs. Since an SN has a width of 16 bits, we limit the number of
Blocks in the region to 2<sup>15</sup> so that the following holds:

Let N equal the number of Blocks in the region. Given Block A with SN a and
Block B with SN b, the most recent Block is:

- Block A when 0 < (a - b) mod 2<sup>16</sup> < N
- Block B when (a - b) mod 2<sup>16</sup> > N
- Undetermined otherwise

We copy the Data from the most recent Block or return an error if none are
found.

To save new Data, we look for the next writable (i.e. erased) Block after the
most recent Block (wrapping around the region if necessary) and write it there
along with the incremented SN and a CRC. If there are no writable Blocks, we
erase the next Page after the most recent Block's Page and write to the first
Block in that Page. Thus all Blocks in the region are written in round-robin
fashion, achieving memory wear-leveling.

If there are at least two Pages in the region then the save procedure is
fault-tolerant, since any erase operation will always happen to a different
Page than the current Block and a fault during the new Block write will cause
the Block's CRC to be mismatched, invalidating it and leaving the current Block
intact.


## Usage

First, we add the library to our C++ source with a single include directive:

```C++
#include "persist/persist.h"
```

### Instantiation

Persist is provided as a C++ class template:

```C++
template <typename NVMem, typename TData, uint8_t datatype_version,
    bool assert_fault_tolerant = true>
class Persist
{
    Persist(NVMem& nvmem) : nvmem_{nvmem} {}

    // ...
};
```

`NVMem` is a driver class used by `Persist` to access nonvolatile memory. A
[template interface](inc/nvmem_template.h) is provided for adaption.

The constructor parameter `nvmem` is an `NVMem` object which we inject into
`Persist` upon instantiation.

`TData` is the data type we want to save and load. This might be a structure
containing calibration data or user settings. It must be both a trivial and
a standard-layout type.

`datatype_version` is a version number which `Persist` uses to prevent loading
invalid data. Version numbers need not be sequential, only unique.

The optional parameter `assert_fault_tolerant` determines whether
fault-tolerance is guaranteed. It is `true` by default, in which case
compilation will fail if the provided `NVMem` type cannot guarantee
fault-tolerance.

Here's how we might instantiate our `Persist` object:

```C++
FlashMemory nvmem;
persist::Persist<FlashMemory, MyDataType, 0> persist{nvmem};
```

`Persist` is abstracted from the details of its underlying memory. If we later
decide we want to use e.g. EEPROM instead of flash, we need only change the
`NVMem` type:

```C++
EEPROM nvmem;
persist::Persist<EEPROM, MyDataType, 0> persist{nvmem};
```

### Initialization

We must initialize the object before using it by calling its `Init` function. If
the `NVMem` object requires initialization, it must be done beforehand:

```C++
nvmem.Init(); // If applicable
persist::Result result = persist.Init();
```

The return value, of type `Result`, may be one of the following:

- `RESULT_SUCCESS`: Successfully initialized.
- `RESULT_FAIL_READ`: Failed to read from memory.

### Loading data

Now we can instantiate a `TData` object and load our stored data:

```C++
MyDataType data;
persist::Result result = persist.Load(data);
```

The return value may be one of the following:

- `RESULT_SUCCESS`: Successfully loaded saved data.
- `RESULT_FAIL_NO_DATA`: No valid saved data was found in the memory region.

### Backward compatibility

We can use the template parameter `datatype_version` and the template member
function `LoadLegacy` to implement backward compatibility if a software update
changes the data type. First, we define a structure for each data type. Each
structure except the lowest priority one must have a defaulted default
constructor and a converting or explicit constructor for the next-lower
priority type:

```C++
struct DataVersion0
{
    uint8_t number;
};

struct DataVersion1
{
    uint16_t number;
    DataVersion1() = default;
    explicit DataVersion1(const DataVersion0& data0)
    {
        number = data0.number;
    }
};

struct DataVersion2
{
    uint32_t number;
    DataVersion2() = default;
    explicit DataVersion2(const DataVersion1& data1)
    {
        number = data1.number;
    }
};
```

Optionally and for convenience, we next define a `Persist` type for each data
type:

```C++
using Persist0 = persist::Persist<FlashMemory, DataVersion0, 0>;
using Persist1 = persist::Persist<FlashMemory, DataVersion1, 1>;
using Persist2 = persist::Persist<FlashMemory, DataVersion2, 2>;
```

Then, after instantiating and initializing a `Persist` object for the highest
priority data type, we call its `LoadLegacy` function with a template argument
list of the other `Persist` types in order of descending priority:

```C++
Persist2 persist{nvmem};
nvmem.Init();
persist.Init();
DataVersion2 data;
persist::Result result = persist.LoadLegacy<Persist1, Persist0>(data);
```

`Persist` will look for saved data of each type in descending priority order.
If any is found, it will be incrementally converted up to the highest priority
type.

The return value may be one of the following:

- `RESULT_SUCCESS`: Successfully loaded saved data.
- `RESULT_FAIL_READ`: Failed to read from memory.
- `RESULT_FAIL_NO_DATA`: No valid saved data was found in the memory region.

### Saving data

To save data back to nonvolatile memory, use the `Save` function.

```C++
persist::Result result = persist.Save(data);
```

The return value may be one of the following:

- `RESULT_SUCCESS`: Successfully loaded saved data.
- `RESULT_FAIL_ERASE`: Failed to erase memory.
- `RESULT_FAIL_WRITE`: Failed to write to memory.


## Example implementations

Demos, example implementations, and unit tests can be found here:

https://github.com/float32/persist-demo

---

Copyright 2023 Tyler Coy

https://www.alrightdevices.com

https://github.com/float32
