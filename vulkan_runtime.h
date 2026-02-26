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

typedef struct
{
    VkBuffer buffer;
    VkDeviceMemory memory; 
    VkDeviceSize size;
    void* mapped;
} VulkanBuffer;

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
    uint32_t bufferCount;
    const VkDescriptorBufferInfo* buffers;
    uint32_t imageCount;
    const VkDescriptorImageInfo* images;
    const void* pushConstants;
    uint32_t pushConstantsSize;
} VulkanKernelArgs;

VkResult vulkanMalloc(VulkanBuffer* buffer, VkDeviceSize size);
VkResult vulkanMemcpy(void* dst, const void* src, size_t len, vulkanMemcpyKind kind);
VkResult vulkanMemset(VulkanBuffer* ptr, int value, VkDeviceSize count);

#define VK_EXPAND(x) x

#define VK_COUNT_ARGS_IMPL( \
     _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, _10, \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
    N, ...) N

#define VK_COUNT_ARGS(...) \
    VK_EXPAND(VK_COUNT_ARGS_IMPL(__VA_ARGS__, \
        20,19,18,17,16,15,14,13,12,11, \
        10,9,8,7,6,5,4,3,2,1,0))

// public header
#define vulkanKernelLaunch(kernel,      \
                           block_x, block_y, block_z, \
                           grid_x,  grid_y,  grid_z,  \
                           smem, stream,              \
                           ...)                       \
    vulkanKernelLaunchImpl(                          \
        (kernel),                                    \
        (block_x), (block_y), (block_z),             \
        (grid_x),  (grid_y),  (grid_z),              \
        (smem),                                      \
        (stream),                                    \
        VK_COUNT_ARGS(__VA_ARGS__),                  \
        __VA_ARGS__                                  \
    )

VkResult vulkanDeviceSyncronize();