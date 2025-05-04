#pragma once

#include "vk.h"

EXTERN_C b32 create_window(vulkan_context *vk, char *title, int width, int height);
EXTERN_C b32 get_window_dimensions(vulkan_context *vk, int *width, int *height);
EXTERN_C b32 window_should_close(void);

EXTERN_C void initialize_imgui(vulkan_context *vk);
EXTERN_C void draw_imgui(vulkan_context *vk, VkCommandBuffer cmd, VkImageView target);
EXTERN_C void deinitialize_imgui(vulkan_context *vk);
