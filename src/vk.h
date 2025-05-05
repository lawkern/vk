#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "dependencies/vk_mem_alloc.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define countof(array) (sizeof(array) / sizeof((array)[0]))
#define VK_CHECK(result)                                                \
   do {                                                                 \
      VkResult err = (result);                                          \
      if(err != VK_SUCCESS)                                             \
      {                                                                 \
         fprintf(stderr, "Vulkan Error: %s\n", string_VkResult(err));   \
         __builtin_trap();                                              \
      }                                                                 \
   } while(0)

#ifdef __cplusplus
#   define EXTERN_C extern "C"
#else
#   define EXTERN_C
#endif

#include <stdint.h>
typedef int32_t b32;
typedef uint32_t u32;
typedef uint64_t u64;

#include <stddef.h>
typedef ptrdiff_t memory_index;

typedef struct {
   char *begin;
   char *end;
} memory_arena;

#define allocate(arena, count, type) (type *)allocate_(arena, count, sizeof(type), _Alignof(type))

static inline void *allocate_(memory_arena *arena, memory_index count, memory_index size, memory_index alignment)
{
    memory_index padding = -(uintptr_t)arena->begin & (alignment - 1);
    assert(count < (arena->end - arena->begin - padding)/size);

    void *result = arena->begin + padding;
    arena->begin += padding + count*size;

    return memset(result, 0, count*size);
}

typedef struct {
   float x, y, z, w;
} vec4;

typedef struct {
   vec4 data[4];
} compute_push_constants;

typedef struct {
   char *name;
   VkPipeline pipeline;
   VkPipelineLayout layout;
   compute_push_constants constants;
} compute_effect;

typedef struct {
   VkImage image;
   VkImageView view;
   VmaAllocation allocation;
   VkExtent3D extent;
   VkFormat format;
} vulkan_image;

typedef struct {
   VkCommandPool pool;
   VkCommandBuffer commands;

   VkSemaphore swapchain_semaphore;
   VkSemaphore render_semaphore;
   VkFence render_fence;
} vulkan_frame_commands;

typedef struct {
   VkPipelineShaderStageCreateInfo shader_stages[2];
   VkPipelineInputAssemblyStateCreateInfo input_assembly;
   VkPipelineRasterizationStateCreateInfo rasterizer;
   VkPipelineColorBlendAttachmentState color_blend_attachment;
   VkPipelineMultisampleStateCreateInfo multisampling;
   VkPipelineLayout layout;
   VkPipelineDepthStencilStateCreateInfo depth_stencil;
   VkPipelineRenderingCreateInfo rendering_info;
   VkFormat color_attachment_format;
} vulkan_pipeline_configuration;

typedef struct {
   VkInstance instance;
   VkPhysicalDevice gpu;
   VkDevice device;

   void *window;
   VkSurfaceKHR surface;

   VkSwapchainKHR swapchain;
   VkExtent2D swapchain_extent;
   VkFormat swapchain_image_format;

   u32 swapchain_image_count;
   VkImage *swapchain_images;
   VkImageView *swapchain_image_views;

   u64 frame_count;
   vulkan_frame_commands frame_commands[2];

   VkQueue graphics_queue;
   VkQueue present_queue;

   VmaAllocator allocator;
   vulkan_image draw_image;
   VkExtent2D draw_extent;

   VkFence immediate_fence;
   VkCommandBuffer immediate_command_buffer;

   compute_effect background_effect;
   VkPipeline triangle_pipeline;
} vulkan_context;
