#include "vulkan_runtime.h"

#include <stdio.h>
#include <inttypes.h>

// ERROR HANDLING

static void vk_log_error(const char* expr, VkResult result, const char* file, int line)
{
    fprintf(stderr, "Vulkan call failed: %s returned %d at %s:%d\n", expr, result, file, line);
}

#define VK_CHECK_RET(expr)                                              \
    do {                                                                \
        VkResult _vk_result = (expr);                                   \
        if (_vk_result != VK_SUCCESS) {                                 \
            vk_log_error(#expr, _vk_result, __FILE__, __LINE__);        \
            return _vk_result;                                          \
        }                                                               \
    } while (0)

// FUNCTIONS

static VkResult createVulkanInstance()
{
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "vulkan_rt",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "vulkan_rt",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2, // or 1_1 / 1_3 etc
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,     // or validation layers if you want them
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = 0, // or instance extensions
        .ppEnabledExtensionNames = NULL,
    };

    VK_CHECK_RET(vkCreateInstance(&createInfo, NULL, &instance));

    return VK_SUCCESS;
}

static VkResult getFirstPhysicalDevice()
{
    uint32_t deviceCount = 0;
    VK_CHECK_RET(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    if (deviceCount == 0) 
    {
        fprintf(stderr, "No Vulkan-capable device found...");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPhysicalDevice physicalDevices[deviceCount];
    VK_CHECK_RET(vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices));

    physicalDevice = physicalDevices[0];

    return VK_SUCCESS;
}

static VkResult getComputeFamily(uint32_t* computeQueueFamily)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

    if (queueFamilyCount == 0) {
        fprintf(stderr, "No queue families found\n");
        return VK_ERROR_UNKNOWN;
    }

    VkQueueFamilyProperties queueFamilies[32];
    if (queueFamilyCount > 32) queueFamilyCount = 32;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    *computeQueueFamily = UINT32_MAX;

    // 1) Try to find a *dedicated* compute queue (async compute):
    //    compute bit set, graphics bit NOT set.
    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        VkQueueFlags flags = queueFamilies[i].queueFlags;

        if ((flags & VK_QUEUE_COMPUTE_BIT) &&
            !(flags & VK_QUEUE_GRAPHICS_BIT))
        {
            *computeQueueFamily = i;
            break;
        }
    }

    // 2) Fallback: any compute-capable queue family (may share with graphics).
    if (*computeQueueFamily == UINT32_MAX) {
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                *computeQueueFamily = i;
                break;
            }
        }
    }

    if (*computeQueueFamily == UINT32_MAX) {
        fprintf(stderr, "No compute-capable queue family found\n");
        return VK_ERROR_UNKNOWN; // or VK_ERROR_FEATURE_NOT_PRESENT
    }

    return VK_SUCCESS;
}

static VkResult getTransferFamily(uint32_t* transferQueueFamily)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

    if (queueFamilyCount == 0) {
        fprintf(stderr, "No queue families found\n");
        return VK_ERROR_UNKNOWN;
    }

    VkQueueFamilyProperties queueFamilies[32];
    if (queueFamilyCount > 32) queueFamilyCount = 32;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    *transferQueueFamily = UINT32_MAX;

    // We want queues with *exactly* TRANSFER + SPARSE_BINDING and nothing else.
    const VkQueueFlags requiredFlags = VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT;

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        VkQueueFlags flags = queueFamilies[i].queueFlags;

        // Must have both TRANSFER and SPARSE_BINDING:
        if ((flags & requiredFlags) != requiredFlags)
            continue;

        // And must have NO OTHER bits set (no graphics, compute, etc.):
        if ((flags & ~requiredFlags) != 0)
            continue;

        *transferQueueFamily = i;
        break;
    }

    if (*transferQueueFamily == UINT32_MAX) {
        fprintf(stderr, "No queue family with exactly TRANSFER | SPARSE_BINDING found\n");
        return VK_ERROR_UNKNOWN; // or VK_ERROR_FEATURE_NOT_PRESENT
    }

    return VK_SUCCESS;
}

static VkResult createLogicalDevice()
{
    // 1. Find compute queue family (required)
    VK_CHECK_RET(getComputeFamily(&computeQueueFamily));

    // 2. Try to find our strict transfer+sparse queue
    VkResult res = getTransferFamily(&transferQueueFamily);
    if (res != VK_SUCCESS)
    {
        // No dedicated pure transfer+sparse queue – fall back to compute family
        transferQueueFamily = computeQueueFamily;
    }

    float queuePriorities[2] = { 1.0f, 1.0f };

    VkDeviceQueueCreateInfo queueInfos[2];
    uint32_t queueInfoCount = 0;

    // Case A: compute and transfer are the same family
    if (transferQueueFamily == computeQueueFamily)
    {
        // Request 2 queues from that family so we can have
        // separate handles (computeQueue and transferQueue).
        queueInfos[queueInfoCount++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = NULL,
            .flags            = 0,
            .queueFamilyIndex = computeQueueFamily,
            .queueCount       = 2,
            .pQueuePriorities = queuePriorities,
        };

        computeQueueIndex  = 0;
        transferQueueIndex = 1;
    }
    else
    {
        // Case B: different families

        // Compute family: 1 queue
        queueInfos[queueInfoCount++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = NULL,
            .flags            = 0,
            .queueFamilyIndex = computeQueueFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriorities[0],
        };

        // Transfer family: 1 queue
        queueInfos[queueInfoCount++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = NULL,
            .flags            = 0,
            .queueFamilyIndex = transferQueueFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriorities[0],
        };

        computeQueueIndex  = 0;
        transferQueueIndex = 0;
    }

    // TODO: enable optional features if needed
    VkPhysicalDeviceFeatures features = {0};

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = NULL,
        .flags                   = 0,
        .queueCreateInfoCount    = queueInfoCount,
        .pQueueCreateInfos       = queueInfos,
        .enabledLayerCount       = 0,
        .ppEnabledLayerNames     = NULL,
        .enabledExtensionCount   = 0,
        .ppEnabledExtensionNames = NULL,
        .pEnabledFeatures        = &features,
    };

    VK_CHECK_RET(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device));

    return VK_SUCCESS;
}

static VkResult getComputeQueueHandle()
{
    vkGetDeviceQueue(device, computeQueueFamily, computeQueueIndex, &computeQueue);
    return VK_SUCCESS;
}

static VkResult getTransferQueueHandle()
{
    vkGetDeviceQueue(device, transferQueueFamily, transferQueueIndex, &transferQueue);
    return VK_SUCCESS;
}

static int findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) 
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) 
        {
            return i;
        }
    }

    fprintf(stderr, "Failed to find suitable memory type!");
    return -1;
}

static VkResult createCommandPools(void)
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };

    // Transfer pool
    poolInfo.queueFamilyIndex = transferQueueFamily;
    VK_CHECK_RET(vkCreateCommandPool(device, &poolInfo, NULL, &transferCommandPool));

    // If families differ, create a separate compute pool
    if (computeQueueFamily != transferQueueFamily) {
        poolInfo.queueFamilyIndex = computeQueueFamily;
        VK_CHECK_RET(vkCreateCommandPool(device, &poolInfo, NULL, &computeCommandPool));
    } else {
        // Same family, you can reuse transferCommandPool for compute too
        computeCommandPool = transferCommandPool;
    }

    return VK_SUCCESS;
}

static VkResult createTransferBuffer()
{
    memset(&transferBuffer, 0, sizeof(VulkanBuffer));
    transferBuffer.size = (VkDeviceSize)TRANSFER_BUFFER_SIZE;

    // Create the buffer
    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = transferBuffer.size;
    bufferInfo.usage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT  |    // to copy into it
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT;      // to copy out of it
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RET(vkCreateBuffer(device, &bufferInfo, NULL, &transferBuffer.buffer));

    // Get memory requirements
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, transferBuffer.buffer, &memReq);

    // Allocate transfer buffer HOST_VISIBLE | HOST_COHERENT | HOST_CACHED
    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    
    // Find the right memory type
    int memoryTypeIdx = findMemoryType(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
        VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    );

    // Fallback without HOST_CACHED
    if (memoryTypeIdx == -1) {
        memoryTypeIdx = findMemoryType(
            memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
    }

    if (memoryTypeIdx == -1) {
        return VK_ERROR_UNKNOWN; // or VK_ERROR_FEATURE_NOT_PRESENT;
    }

    allocInfo.memoryTypeIndex = (uint32_t)memoryTypeIdx;

    allocInfo.memoryTypeIndex = memoryTypeIdx;

    VK_CHECK_RET(vkAllocateMemory(device, &allocInfo, NULL, &transferBuffer.memory));

    // Bind buffer to memory
    VK_CHECK_RET(vkBindBufferMemory(device, transferBuffer.buffer, transferBuffer.memory, 0));

    // Keep it permanetly mapped
    vkMapMemory(device, transferBuffer.memory, 0, transferBuffer.size, 0, &transferBuffer.mapped);

    return VK_SUCCESS;
}

static VkResult createCommandBufferTransfers(void)
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = NULL,
        .commandPool        = transferCommandPool,          // must use transfer queue family
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VK_CHECK_RET(vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer));
    
    return VK_SUCCESS;
}

static VkResult createCommandBufferKernels(void)
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = NULL,
        .commandPool        = computeCommandPool,           // must use compute queue family
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VK_CHECK_RET(vkAllocateCommandBuffers(device, &allocInfo, &computeCommandBuffer));

    return VK_SUCCESS;
}

__attribute__((constructor))
static void vulkan_rt_init(void) 
{
    // Create Vulkan instance
    VK_CHECK_RET(createVulkanInstance());

    // Select first physical device available
    VK_CHECK_RET(getFirstPhysicalDevice());
    
    // Create logical device
    VK_CHECK_RET(createLogicalDevice());

    // Get queue handles
    VK_CHECK_RET(getComputeQueueHandle());
    VK_CHECK_RET(getTransferQueueHandle());

    // Create command pools
    VK_CHECK_RET(createCommandPools());

    // Create transfer buffer
    VK_CHECK_RET(createTransferBuffer());

    // Create command buffers 
    VK_CHECK_RET(createCommandBufferTransfers());
    VK_CHECK_RET(createCommandBufferKernels());
}

__attribute__((destructor))
static void vulkan_rt_close(void) 
{

}

VkResult vulkanMalloc(VulkanBuffer* buf, VkDeviceSize size)
{
    if (!buf || size == 0) 
    {
        return VK_ERROR_UNKNOWN;
    }

    memset(buf, 0, sizeof(*buf));
    buf->size = (VkDeviceSize)size;

    // Create the buffer
    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = buf->size;
    bufferInfo.usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |   // for compute shaders
        VK_BUFFER_USAGE_TRANSFER_DST_BIT  |    // to copy into it
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT;      // to copy out of it
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK_RET(vkCreateBuffer(device, &bufferInfo, NULL, &buf->buffer));

    // Get memory requirements
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buf->buffer, &memReq);

    // Allocate DEVICE_LOCAL memory
    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    
    // Find the right memory type
    int memoryTypeIdx = findMemoryType(
        memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    if (memoryTypeIdx == -1)
    {
        return VK_ERROR_UNKNOWN;
    }

    allocInfo.memoryTypeIndex = memoryTypeIdx;

    VK_CHECK_RET(vkAllocateMemory(device, &allocInfo, NULL, &buf->memory));

    // Bind buffer to memory
    VK_CHECK_RET(vkBindBufferMemory(device, buf->buffer, buf->memory, 0));

    return VK_SUCCESS;
}

// HELPER MEMCPY
static VkResult recordAndSubmitCopy(
    VkBuffer srcBuffer,
    VkDeviceSize srcOffset,
    VkBuffer dstBuffer,
    VkDeviceSize dstOffset,
    VkDeviceSize size)
{
    // All offsets and size must be multiples of 4 per Vulkan spec
    if ((srcOffset % 4) != 0 || (dstOffset % 4) != 0 || (size % 4) != 0) {
        fprintf(stderr, "recordAndSubmitCopy: offsets/size must be 4-byte aligned\n");
        return VK_ERROR_VALIDATION_FAILED_EXT;
    }

    VkResult res;

    // Reset the command buffer so we can reuse it
    res = vkResetCommandBuffer(transferCommandBuffer, 0);
    if (res != VK_SUCCESS) return res;

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };

    res = vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);
    if (res != VK_SUCCESS) return res;

    VkBufferCopy region = {
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size      = size,
    };

    vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &region);

    res = vkEndCommandBuffer(transferCommandBuffer);
    if (res != VK_SUCCESS) return res;

    // Fence for blocking submit
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    VkFence fence;
    res = vkCreateFence(device, &fenceInfo, NULL, &fence);
    if (res != VK_SUCCESS) return res;

    VkSubmitInfo submitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext              = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores    = NULL,
        .pWaitDstStageMask  = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers    = &transferCommandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores    = NULL,
    };

    res = vkQueueSubmit(transferQueue, 1, &submitInfo, fence);
    if (res != VK_SUCCESS) {
        vkDestroyFence(device, fence, NULL);
        return res;
    }

    res = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, NULL);

    return res;
}

VkResult vulkanMemcpy(void* dst, const void* src, size_t len, vulkanMemcpyKind kind)
{
    if (len == 0) {
        return VK_SUCCESS;
    }

    switch (kind)
    {
        case vulkanMemcpyHostToHost:
        {
            // trivial CPU memcpy
            memcpy(dst, src, len);
            return VK_SUCCESS;
        }

        case vulkanMemcpyHostToDevice:
        {
            // dst: VulkanBuffer* (device), src: host pointer
            VulkanBuffer* dstBuf = (VulkanBuffer*)dst;
            const uint8_t* srcHost = (const uint8_t*)src;

            if (dstBuf->buffer == VK_NULL_HANDLE) {
                fprintf(stderr, "HostToDevice: dst buffer is null\n");
                return VK_ERROR_VALIDATION_FAILED_EXT;
            }

            VkDeviceSize remaining = (VkDeviceSize)len;
            VkDeviceSize dstOffset = 0;
            VkDeviceSize transferSize = transferBuffer.size;

            while (remaining > 0) {
                VkDeviceSize chunk = remaining < transferSize ? remaining : transferSize;

                // Copy from host memory into mapped transfer buffer
                memcpy((uint8_t*)transferBuffer.mapped, srcHost + dstOffset, (size_t)chunk);

                // GPU-side copy: transferBuffer -> dstBuf
                VkResult res = recordAndSubmitCopy(
                    transferBuffer.buffer, 0,
                    dstBuf->buffer, dstOffset,
                    chunk
                );
                if (res != VK_SUCCESS) return res;

                remaining -= chunk;
                dstOffset += chunk;
            }

            return VK_SUCCESS;
        }

        case vulkanMemcpyDeviceToHost:
        {
            // dst: host pointer, src: VulkanBuffer* (device)
            uint8_t* dstHost = (uint8_t*)dst;
            const VulkanBuffer* srcBuf = (const VulkanBuffer*)src;

            if (srcBuf->buffer == VK_NULL_HANDLE) {
                fprintf(stderr, "DeviceToHost: src buffer is null\n");
                return VK_ERROR_VALIDATION_FAILED_EXT;
            }

            VkDeviceSize remaining = (VkDeviceSize)len;
            VkDeviceSize srcOffset = 0;
            VkDeviceSize transferSize = transferBuffer.size;

            while (remaining > 0) {
                VkDeviceSize chunk = remaining < transferSize ? remaining : transferSize;

                // GPU-side copy: srcBuf -> transferBuffer
                VkResult res = recordAndSubmitCopy(
                    srcBuf->buffer, srcOffset,
                    transferBuffer.buffer, 0,
                    chunk
                );
                if (res != VK_SUCCESS) return res;

                // Copy from mapped transfer buffer into host memory
                memcpy(dstHost + srcOffset, (uint8_t*)transferBuffer.mapped, (size_t)chunk);

                remaining -= chunk;
                srcOffset += chunk;
            }

            return VK_SUCCESS;
        }

        case vulkanMemcpyDeviceToDevice:
        {
            // dst: VulkanBuffer*, src: VulkanBuffer*
            VulkanBuffer* dstBuf = (VulkanBuffer*)dst;
            const VulkanBuffer* srcBuf = (const VulkanBuffer*)src;

            if (dstBuf->buffer == VK_NULL_HANDLE || srcBuf->buffer == VK_NULL_HANDLE) {
                fprintf(stderr, "DeviceToDevice: src or dst buffer is null\n");
                return VK_ERROR_VALIDATION_FAILED_EXT;
            }

            VkDeviceSize remaining = (VkDeviceSize)len;
            VkDeviceSize offset    = 0;

            // We *could* use transferBuffer to chunk if extremely large, but
            // we can also just copy directly device->device in chunks.
            VkDeviceSize maxChunk = transferBuffer.size; // reuse this as an upper bound

            while (remaining > 0) {
                VkDeviceSize chunk = remaining < maxChunk ? remaining : maxChunk;

                VkResult res = recordAndSubmitCopy(
                    srcBuf->buffer, offset,
                    dstBuf->buffer, offset,
                    chunk
                );
                if (res != VK_SUCCESS) return res;

                remaining -= chunk;
                offset    += chunk;
            }

            return VK_SUCCESS;
        }

        default:
            fprintf(stderr, "vulkanMemcpy: invalid memcpy kind\n");
            return VK_ERROR_VALIDATION_FAILED_EXT;
    }
}

// HELPER MEMSET
static VkResult recordAndSubmitFill(
    VkBuffer buffer,
    VkDeviceSize offset,
    VkDeviceSize size,
    uint32_t pattern)
{
    // Vulkan requires offset and size to be multiples of 4
    if ((offset % 4) != 0 || (size % 4) != 0) {
        fprintf(stderr, "recordAndSubmitFill: offset/size must be 4-byte aligned\n");
        return VK_ERROR_VALIDATION_FAILED_EXT;
    }

    VkResult res;

    // Reset the command buffer
    res = vkResetCommandBuffer(transferCommandBuffer, 0);
    if (res != VK_SUCCESS) return res;

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };

    res = vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);
    if (res != VK_SUCCESS) return res;

    vkCmdFillBuffer(transferCommandBuffer, buffer, offset, size, pattern);

    res = vkEndCommandBuffer(transferCommandBuffer);
    if (res != VK_SUCCESS) return res;

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };

    VkFence fence;
    res = vkCreateFence(device, &fenceInfo, NULL, &fence);
    if (res != VK_SUCCESS) return res;

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &transferCommandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };

    res = vkQueueSubmit(transferQueue, 1, &submitInfo, fence);
    if (res != VK_SUCCESS) {
        vkDestroyFence(device, fence, NULL);
        return res;
    }

    res = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, NULL);

    return res;
}

VkResult vulkanMemset(VulkanBuffer* ptr, int value, VkDeviceSize count)
{
    if (count == 0 || ptr == NULL) {
        return ptr ? ptr->buffer : VK_NULL_HANDLE;
    }

    if (count > ptr->size) {
        fprintf(stderr, "vulkanMemset: count (%" PRIu64 ") > buffer size (%" PRIu64 ")\n",
                (uint64_t)count, (uint64_t)ptr->size);
        return VK_NULL_HANDLE;
    }

    // If buffer is host-visible and mapped, do it on CPU.
    if (ptr->mapped != NULL) {
        memset(ptr->mapped, value, (size_t)count);
        return ptr->buffer;
    }

    // Device-local buffer: use vkCmdFillBuffer via transfer queue.
    // vkCmdFillBuffer uses a 32-bit pattern; memset uses 8-bit value.
    // Build a 32-bit pattern with the byte repeated: 0xVVVVVVVV.
    uint8_t v8 = (uint8_t)value;
    uint32_t pattern = 0x01010101u * (uint32_t)v8;

    // Vulkan requires size to be multiple of 4 for vkCmdFillBuffer.
    if ((count % 4) != 0) {
        fprintf(stderr, "vulkanMemset: count (%" PRIu64 ") must be 4-byte aligned for device-local memset\n",
                (uint64_t)count);
        return VK_NULL_HANDLE;
    }

    VkResult res = recordAndSubmitFill(ptr->buffer, 0, count, pattern);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "vulkanMemset: vkCmdFillBuffer failed (VkResult = %d)\n", res);
        return VK_NULL_HANDLE;
    }

    return ptr->buffer;
}

VkResult vulkanKernelLaunch(
    const char* kernel, 
    uint32_t block_x, uint32_t block_y, uint32_t block_z,
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    uint64_t smem,
    uint32_t stream,
    VulkanKernelArgs args
)
{

}

VkResult vulkanDeviceSyncronize()
{

}