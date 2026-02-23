#include "vulkan_runtime.h"

#include <stdio.h>

__attribute__((constructor))
static void vulkan_rt_init(void) 
{
    // Create Vulkan instance
    VkResult res = VK_SUCCESS;
    res = vkCreateInstance(NULL, NULL, &instance);
    if (res != VK_SUCCESS) 
    {
        fprintf(stderr, "Couldn't create the Vulkan instance...");
    }

    // Select first physical device available
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    if (device_count == 0) 
    {
        fprintf(stderr, "No Vulkan-capable device found...");
    }

    VkPhysicalDevice physical_devices[device_count];
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices);

    physical_device = physical_devices[0];

    // Find the right queue for the job
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

    VkQueueFamilyProperties queue_families[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

    int comnpute_family = -1;
    for (uint32_t i = 0; i < queue_family_count; ++i)
    {
        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            comnpute_family = (int)(i);
            break;
        }
    }

    if (comnpute_family == -1) 
    {
        fprintf(stderr, "No compute queue found...");
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info;
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = comnpute_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo device_create_info;
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    // Enable device features, extensions, etc.
    // device_create_info.enabledExtensionCount = 0;
    // device_create_info.ppEnabledExtensionNames = NULL;

    res = vkCreateDevice(physical_device, &device_create_info, NULL, &device);
    if (res != VK_SUCCESS) 
    {
        fprintf(stderr, "Couldn't create the Logical Device...");
    }

    // Get the queue handle
    vkGetDeviceQueue(device, comnpute_family, 0, &compute_queue);
}

__attribute__((destructor))
static void vulkan_rt_close(void) 
{

}

int vulkanMalloc(void** ptr, size_t len)
{
    
}

int vulkanMemcpy(void* dst, void* src, size_t len, vulkanMemcpyKind kind)
{

}

int vulkanMemset(void* devPtr, int value, size_t count)
{

}

int vulkanKernelLaunch()
{

}

int vulkanDeviceSyncronize()
{

}