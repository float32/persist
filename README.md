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
containing calibration data or user settings.

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

In the case of `RESULT_FAIL_NO_DATA`, we can use the template parameter
`datatype_version` to implement backward compatibility if a software update
changes the data type:

```C++
persist::Persist<FlashMemory, NewDataType, 1> persist{nvmem};
nvmem.Init();
persist.Init();
NewDataType new_data;

if (persist.Load(new_data) == persist::RESULT_FAIL_NO_DATA)
{
    persist::Persist<FlashMemory, OldDataType, 0> old_persist{nvmem};
    old_persist.Init();
    OldDataType old_data;

    if (old_persist.Load(old_data) == persist::RESULT_SUCCESS)
    {
        new_data = MigrateData(old_data);
    }
}
```

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
