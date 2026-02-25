#pragma once

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <stddef.h>
#include <cuda_runtime.h>

#define TRANSFER_BUFFER_SIZE (8 * 1024 * 1024) // 8 MB

#define VK_CHECK_FATAL(expr)                                            \
    do {                                                                \
        VkResult _vk_result = (expr);                                   \
        if (_vk_result != VK_SUCCESS) {                                 \
            fprintf(stderr, "Fatal Vulkan error %d from %s at %s:%d\n", \
                    _vk_result, #expr, __FILE__, __LINE__);             \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
    } while (0)

typedef enum
{
    vulkanMemcpyHostToHost = 0,
    vulkanMemcpyHostToDevice,
    vulkanMemcpyDeviceToHost,
    vulkanMemcpyDeviceToDevice
} vulkanMemcpyKind;

// Library vars
static VkInstance instance;

static VkPhysicalDevice physicalDevice;

static uint32_t computeQueueFamily = UINT32_MAX;
static uint32_t transferQueueFamily = UINT32_MAX;

static uint32_t computeQueueIndex   = 0;
static uint32_t transferQueueIndex  = 0;

static VkDevice device;

static VkQueue transferQueue;
static VkQueue computeQueue;

static VkCommandPool transferCommandPool;
static VkCommandPool computeCommandPool;

static VulkanBuffer transferBuffer;

static VkCommandBuffer transferCommandBuffer;
static VkCommandBuffer computeCommandBuffer;

typedef struct
{
    VkBuffer buffer;
    VkDeviceMemory memory; 
    VkDeviceSize size;
    void* mapped;
} VulkanBuffer;

typedef struct
{
    
} VulkanKernelArgs;

VkResult vulkanMalloc(VulkanBuffer* buffer, VkDeviceSize size);
VkResult vulkanMemcpy(void* dst, const void* src, size_t len, vulkanMemcpyKind kind);
VkResult vulkanMemset(VulkanBuffer* ptr, int value, VkDeviceSize count);

VkResult vulkanKernelLaunch(
    const char* kernel, 
    uint32_t block_x, uint32_t block_y, uint32_t block_z,
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    uint64_t smem,
    uint32_t stream,
    VulkanKernelArgs args
);
VkResult vulkanDeviceSyncronize();