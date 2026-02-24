#pragma once

#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <stddef.h>

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

static uint32_t computeQueueFamily;
static uint32_t transferQueueFamily;

static VkDevice device;

static VkQueue transferQueue;
static VkQueue computeQueue;

static VulkanBuffer transferBuffer;

static VkCommandPool transferCommandPool;
static VkCommandBuffer transferCommandBuffer;

typedef struct 
{
    VkBuffer buffer;
    VkDeviceMemory memory; 
    VkDeviceSize size;
    void* mapped;
} VulkanBuffer;

VkResult vulkanMalloc(VulkanBuffer* buffer, VkDeviceSize size);
VkBuffer vulkanMemcpy(VulkanBuffer* dst, void* src, size_t len, vulkanMemcpyKind kind);
VkBuffer vulkanMemset(VulkanBuffer* ptr, int value, VkDeviceSize count);

VkResult vulkanKernelLaunch();
VkResult vulkanDeviceSyncronize();