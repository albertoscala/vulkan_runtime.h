#include "vulkan_runtime.h"

#include <stdio.h>
#include <vulkan/vulkan.h>

__attribute__((constructor))
static void mylib_init(void) {
    // This runs automatically when the .so / .dylib is loaded
    // i.e. before main() and before any user function in this lib
    fprintf(stderr, "[mylib] init called!\n");
}

__attribute__((destructor))
static void mylib_fini(void) {
    // This runs when the process is shutting down (or dlclose)
    fprintf(stderr, "[mylib] fini called!\n");
}

int vulkanMalloc(void** ptr, size_t len)
{

}

int vulkanMemcpy(void* dst, void* src, size_t len, cudaMemcpyKind kind)
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