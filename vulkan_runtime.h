#pragma once

#include <stddef.h>

enum cudaMemcpyKind
{
    vulkanMemcpyHostToHost = 0,
    vulkanMemcpyHostToDevice,
    vulkanMemcpyDeviceToHost,
    vulkanMemcpyDeviceToDevice
};

int vulkanMalloc(void** ptr, size_t len);
int vulkanMemcpy(void* dst, void* src, size_t len, cudaMemcpyKind kind);
int vulkanMemset(void* devPtr, int value, size_t count);

int vulkanKernelLaunch();
int vulkanDeviceSyncronize();