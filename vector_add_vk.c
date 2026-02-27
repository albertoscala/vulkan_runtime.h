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

    for (uint32_t i = 0; i < SIZE; i++)
    {
        arr1[i] = i; 
        arr2[i] = i * 2;
    }

    VK_CHECK_FATAL(vulkanMalloc(&d_i32_arr1, sizeof(int) * SIZE));
    VK_CHECK_FATAL(vulkanMalloc(&d_i32_arr2, sizeof(int) * SIZE));
    VK_CHECK_FATAL(vulkanMalloc(&d_i32_arr3, sizeof(int) * SIZE));
    VK_CHECK_FATAL(vulkanMalloc(&d_i32_n, sizeof(int)));

    vulkanMemcpy(&d_i32_arr1, arr1, sizeof(int) * SIZE, vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_arr2, arr2, sizeof(int) * SIZE, vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_n, &n, sizeof(int), vulkanMemcpyHostToDevice);
    
    vulkanMemset(&d_i32_arr3, 0, sizeof(uint32_t) * SIZE);

    vulkanKernelLaunch(
        "vector_add.spv", 
        SIZE, 1, 1, 
        1, 1, 1,
        0,
        0,
        &d_i32_arr1, &d_i32_arr2, &d_i32_arr3, &d_i32_n
    );
    vulkanDeviceSyncronize();

    vulkanMemcpy(arr3, &d_i32_arr3, sizeof(int) * SIZE, vulkanMemcpyDeviceToHost);

    for (uint32_t i = 0; i < SIZE; i++)
    {
        printf("IDX %d %d+%d=%d\n", i, arr1[i], arr2[i], arr3[i]);
    }

    return 0;
}