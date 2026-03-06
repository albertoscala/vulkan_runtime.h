# vulkan_runtime.h

`vulkan_runtime.h` is a small C runtime that provides a CUDA-like
workflow for launching Vulkan compute shaders with minimal boilerplate.

The project wraps common Vulkan setup and memory operations behind a
compact API, so simple compute workloads can be written in a style that
feels closer to GPU runtime code than raw Vulkan. The repository
currently includes examples for vector addition and matrix
multiplication, along with GLSL compute shaders compiled to SPIR-V.

## Overview

This library is designed for straightforward Vulkan compute experiments
where you want:

-   device buffer allocation
-   host/device memory copies
-   buffer initialization
-   simple kernel launches
-   very little Vulkan setup code in application code

At the API level, the public entry points exposed in `vulkan_runtime.h`
are:

-   `vulkanMalloc`
-   `vulkanMemcpy`
-   `vulkanMemset`
-   `vulkanKernelLaunch`
-   `vulkanDeviceSyncronize`

The runtime also defines a `VulkanBuffer` type and a `vulkanMemcpyKind`
enum with host-to-host, host-to-device, device-to-host, and
device-to-device copy modes.

## Repository Layout

    .
    ├── vulkan_runtime.h
    ├── vulkan_runtime.c
    ├── main.c
    ├── kernel.comp
    ├── vector_add.comp
    ├── vector_add_vk.c
    ├── matmul.comp
    ├── matmul_vk.c
    └── comp.sh

The repository contains a minimal `main.c` example that allocates
buffers, uploads inputs, launches a compute shader (`kernel.spv`),
synchronizes, copies results back, and prints them.

## Requirements

You will need:

-   a Vulkan-capable GPU and Vulkan driver
-   Vulkan headers and loader (`vulkan/vulkan.h`, `-lvulkan`)
-   `glslc` to compile GLSL compute shaders into SPIR-V
-   a C compiler such as `gcc`

The current header also includes `cuda_runtime.h`, so your environment
may need CUDA headers available during compilation unless that include
is removed or replaced.

## Build

Example build steps:

``` sh
glslc kernel.comp -o kernel.spv
gcc main.c vulkan_runtime.c -lvulkan -o main
```

Run:

``` sh
./main
```

## Programming Model

Typical workflow:

1.  Allocate device buffers with `vulkanMalloc`
2.  Copy host data to device with `vulkanMemcpy`
3.  Initialize buffers with `vulkanMemset` if needed
4.  Launch a compute shader with `vulkanKernelLaunch`
5.  Wait for completion with `vulkanDeviceSyncronize`
6.  Copy results back with `vulkanMemcpy`

## API

### VulkanBuffer

``` c
typedef struct
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    void* mapped;
} VulkanBuffer;
```

Represents a device buffer managed by the runtime.

### vulkanMemcpyKind

``` c
typedef enum
{
    vulkanMemcpyHostToHost = 0,
    vulkanMemcpyHostToDevice,
    vulkanMemcpyDeviceToHost,
    vulkanMemcpyDeviceToDevice
} vulkanMemcpyKind;
```

Specifies the direction of a memory copy.

### vulkanMalloc

``` c
VkResult vulkanMalloc(VulkanBuffer* buffer, VkDeviceSize size);
```

Allocates a Vulkan buffer and its backing memory.

### vulkanMemcpy

``` c
VkResult vulkanMemcpy(void* dst, const void* src, size_t len, vulkanMemcpyKind kind);
```

Copies memory between host and device.

### vulkanMemset

``` c
VkResult vulkanMemset(VulkanBuffer* ptr, int value, VkDeviceSize count);
```

Initializes device memory.

### vulkanKernelLaunch

Launches a compute shader with a CUDA-like syntax.

Example:

``` c
vulkanKernelLaunch(
    "kernel.spv",
    1, 1, 1,
    SIZE, 1, 1,
    0,
    0,
    &d_i32_arr1, &d_i32_arr2, &d_i32_arr3, &d_i32_n
);
```

### vulkanDeviceSyncronize

``` c
VkResult vulkanDeviceSyncronize();
```

Blocks until submitted work completes.

## Example

Minimal vector addition example:

``` c
#include <stdio.h>
#include "vulkan_runtime.h"

#define SIZE 10

int main()
{
    int arr1[SIZE];
    int arr2[SIZE];
    int arr3[SIZE];
    int n = SIZE;

    VulkanBuffer d_i32_arr1;
    VulkanBuffer d_i32_arr2;
    VulkanBuffer d_i32_arr3;
    VulkanBuffer d_i32_n;

    for (uint32_t i = 0; i < SIZE; i++) {
        arr1[i] = i;
        arr2[i] = i * 2;
    }

    vulkanMalloc(&d_i32_arr1, sizeof(int) * SIZE);
    vulkanMalloc(&d_i32_arr2, sizeof(int) * SIZE);
    vulkanMalloc(&d_i32_arr3, sizeof(int) * SIZE);
    vulkanMalloc(&d_i32_n, sizeof(int));

    vulkanMemcpy(&d_i32_arr1, arr1, sizeof(int) * SIZE, vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_arr2, arr2, sizeof(int) * SIZE, vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_n, &n, sizeof(int), vulkanMemcpyHostToDevice);

    vulkanMemset(&d_i32_arr3, 0, sizeof(int) * SIZE);

    vulkanKernelLaunch(
        "kernel.spv",
        1, 1, 1,
        SIZE, 1, 1,
        0,
        0,
        &d_i32_arr1, &d_i32_arr2, &d_i32_arr3, &d_i32_n
    );

    vulkanDeviceSyncronize();

    vulkanMemcpy(arr3, &d_i32_arr3, sizeof(int) * SIZE, vulkanMemcpyDeviceToHost);

    for (uint32_t i = 0; i < SIZE; i++) {
        printf("IDX %d %d+%d=%d\n", i, arr1[i], arr2[i], arr3[i]);
    }

    return 0;
}
```

## License

MIT
