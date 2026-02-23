#include <stdio.h>

#define SIZE 10

int main()
{
    int arr1[SIZE];
    int arr2[SIZE];
    int arr3[SIZE];
    int* d_arr1;
    int* d_arr2;
    int* d_arr3;

    vulkanMalloc(&d_arr1, sizeof(int) * SIZE);
    vulkanMalloc(&d_arr1, sizeof(int) * SIZE);
    vulkanMalloc(&d_arr1, sizeof(int) * SIZE);

    vulkanMemcpy(d_arr1, arr1, sizeof(int) * SIZE, vulkanMemcpyHostToDevice);
    vulkanMemcpy(d_arr2, arr2, sizeof(int) * SIZE, vulkanMemcpyHostToDevice);
    
    vulkanMemset(d_arr3, 0, sizeof(int) * SIZE);

    vulkanKernelLaunch();
    vulkanDeviceSyncronize();

    vulkanMemcpy(arr3, d_arr3, sizeof(int) * SIZE, vulkanMemcpyDeviceToHost);

    return 0;
}