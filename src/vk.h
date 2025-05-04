#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "dependencies/vk_mem_alloc.h"

#include <stdint.h>
typedef int32_t b32;
typedef uint32_t u32;
typedef uint64_t u64;

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
} vulkan_context;
