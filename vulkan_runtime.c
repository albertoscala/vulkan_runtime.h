#include "vulkan_runtime.h"

#include <stdio.h>

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

    VkQueueFamilyProperties queueFamilies[32];
    if (queueFamilyCount > 32) queueFamilyCount = 32;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    *computeQueueFamily = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            *computeQueueFamily = i;
            break;
        }
    }

    if (*computeQueueFamily == UINT32_MAX) {
        fprintf(stderr, "No compute-capable queue family found\n");
        return VK_ERROR_UNKNOWN;
    }

    return VK_SUCCESS;
}

static VkResult getTransferFamily(uint32_t* transferQueueFamily)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

    VkQueueFamilyProperties queueFamilies[32];
    if (queueFamilyCount > 32) queueFamilyCount = 32;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    *transferQueueFamily = UINT32_MAX;

    // Try dedicated transfer queue first
    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        VkQueueFlags flags = queueFamilies[i].queueFlags;
        if ((flags & VK_QUEUE_TRANSFER_BIT) &&
            !(flags & VK_QUEUE_GRAPHICS_BIT) &&
            !(flags & VK_QUEUE_COMPUTE_BIT))
        {
            *transferQueueFamily = i;
            break;
        }
    }

    // If not found, fall back to any transfer-capable family
    if (*transferQueueFamily == UINT32_MAX) {
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                *transferQueueFamily = i;
                break;
            }
        }
    }

    if (*transferQueueFamily == UINT32_MAX) {
        fprintf(stderr, "No transfer-capable queue family found\n");
        return VK_ERROR_UNKNOWN;
    }

    return VK_SUCCESS;
}

static VkResult createLogicalDevice()
{
    // Find compute queue family
    VK_CHECK_RET(getComputeFamily(&computeQueueFamily));

    // Find transfer queue family (or fall back to compute)
    VkResult res = getTransferFamily(&transferQueueFamily);
    if (res != VK_SUCCESS)
    {
        // No dedicated transfer family – just use compute family
        transferQueueFamily = computeQueueFamily;
    }

    float queuePriority = 1.0f;

    // We may need up to 2 queue create infos, one for compute, one for transfer
    VkDeviceQueueCreateInfo queueInfos[2];
    uint32_t queueInfoCount = 0;

    // Always request a queue from the compute family
    queueInfos[queueInfoCount++] = (VkDeviceQueueCreateInfo) {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext            = NULL,
        .flags            = 0,
        .queueFamilyIndex = computeQueueFamily,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority,
    };

    // If transfer family is different, request a queue for it too
    if (transferQueueFamily != computeQueueFamily) {
        queueInfos[queueInfoCount++] = (VkDeviceQueueCreateInfo) {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = NULL,
            .flags            = 0,
            .queueFamilyIndex = transferQueueFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        };
    }

    //TODO: Check about the optional features
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
        .pEnabledFeatures        = &features, // For optional features
    };

    VK_CHECK_RET(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device));

    return VK_SUCCESS;
}

static VkResult getComputeQueueHandle()
{
    vkGetDeviceQueue(device, computeQueueFamily, 0, &computeQueue);
    return VK_SUCCESS;
}

static VkResult getTransferQueueHandle()
{
    vkGetDeviceQueue(device, transferQueueFamily, 0, &transferQueue);
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

VkBuffer vulkanMemcpy(VulkanBuffer* dst, void* src, size_t len, vulkanMemcpyKind kind)
{
    size_t cursor = 0;
    while (cursor < len)
    {
        if (len > TRANSFER_BUFFER_SIZE)
        {
            cursor += TRANSFER_BUFFER_SIZE;
        }
        else
        {
            memcpy(transferBuffer.mapped, src, len);

        }
    }
}

VkBuffer vulkanMemset(VulkanBuffer* ptr, int value, VkDeviceSize count)
{

}

int vulkanKernelLaunch()
{

}

int vulkanDeviceSyncronize()
{

}