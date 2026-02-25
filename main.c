#include <stdio.h>

#include "vulkan_runtime.h"

#define SIZE 10

int main()
{
    int arr1[SIZE];
    int arr2[SIZE];
    int arr3[SIZE];
    VulkanBuffer d_i32_arr1;
    VulkanBuffer d_i32_arr2;
    VulkanBuffer d_i32_arr3;

    VK_CHECK_FATAL(vulkanMalloc(&d_i32_arr1, sizeof(int) * SIZE));
    VK_CHECK_FATAL(vulkanMalloc(&d_i32_arr1, sizeof(int) * SIZE));
    VK_CHECK_FATAL(vulkanMalloc(&d_i32_arr1, sizeof(int) * SIZE));

    vulkanMemcpy(&d_i32_arr1, arr1, sizeof(int) * SIZE, vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_arr2, arr2, sizeof(int) * SIZE, vulkanMemcpyHostToDevice);
    
    // vulkanMemset(d_arr3, 0, sizeof(int) * SIZE);

    // vulkanKernelLaunch();
    // vulkanDeviceSyncronize();

    vulkanMemcpy(arr3, &d_i32_arr3, sizeof(int) * SIZE, vulkanMemcpyDeviceToHost);

    return 0;
}