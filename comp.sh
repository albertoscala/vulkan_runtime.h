glslc kernel.comp -o kernel.spv;
gcc main.c vulkan_runtime.c -lvulkan -o main