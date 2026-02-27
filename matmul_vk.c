#include <stdio.h>

#include "vulkan_runtime.h"

#define M 256
#define K 128
#define N 512

int main()
{
    int A[M * K];
    int B[K * N];
    int C[M * N];
    int m = M;
    int k = K;
    int n = N;
    VulkanBuffer d_i32_A;
    VulkanBuffer d_i32_B;
    VulkanBuffer d_i32_C;
    VulkanBuffer d_i32_m;
    VulkanBuffer d_i32_k;
    VulkanBuffer d_i32_n;

    for (uint32_t i = 0; i < M * K; i++)
    {
        A[i] = i; 
    }

    for (uint32_t i = 0; i < K * N; i++)
    {
        B[i] = i * 2; 
    }

    vulkanMalloc(&d_i32_A, sizeof(int) * M * K);
    vulkanMalloc(&d_i32_B, sizeof(int) * K * N);
    vulkanMalloc(&d_i32_C, sizeof(int) * M * N);
    vulkanMalloc(&d_i32_m, sizeof(int));
    vulkanMalloc(&d_i32_k, sizeof(int));
    vulkanMalloc(&d_i32_n, sizeof(int));

    vulkanMemcpy(&d_i32_A, A, sizeof(int) * M * K, vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_B, B, sizeof(int) * K * N, vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_m, &m, sizeof(int), vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_k, &k, sizeof(int), vulkanMemcpyHostToDevice);
    vulkanMemcpy(&d_i32_n, &n, sizeof(int), vulkanMemcpyHostToDevice);
    
    vulkanMemset(&d_i32_C, 0, sizeof(int) * M * N);

    uint32_t Bx = 16;
    uint32_t By = 16;
    uint32_t groupCountX = (N + Bx - 1) / Bx;
    uint32_t groupCountY = (M + By - 1) / By;

    vulkanKernelLaunch(
        "matmul.spv", 
        Bx, By, 1,
        groupCountX, groupCountY, 1,
        0,
        0,
        &d_i32_A, &d_i32_B, &d_i32_C, 
        &d_i32_m, &d_i32_k, &d_i32_n
    );
    vulkanDeviceSyncronize();

    vulkanMemcpy(C, &d_i32_C, sizeof(int) * M * N, vulkanMemcpyDeviceToHost);

    for (uint32_t i = 0; i < M; i++)
    {
        for (uint32_t j = 0; j < N; j++)
        {
            printf("%d\t", C[i * N + j]);            
        }
        printf("\n");
    }

    return 0;
}