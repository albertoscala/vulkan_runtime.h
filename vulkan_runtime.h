#pragma once

#include <vulkan/vulkan.h>
#include <stddef.h>

typedef enum
{
    vulkanMemcpyHostToHost = 0,
    vulkanMemcpyHostToDevice,
    vulkanMemcpyDeviceToHost,
    vulkanMemcpyDeviceToDevice
} vulkanMemcpyKind;

// Library vars
static VkInstance instance;
static VkPhysicalDevice physical_device;
static VkDevice device;
static VkQueue compute_queue;

int vulkanMalloc(void** ptr, size_t len);
int vulkanMemcpy(void* dst, void* src, size_t len, vulkanMemcpyKind kind);
int vulkanMemset(void* devPtr, int value, size_t count);

int vulkanKernelLaunch();
int vulkanDeviceSyncronize();