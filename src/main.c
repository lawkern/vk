#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <stdint.h>
typedef uint32_t u32;
typedef uint64_t u64;

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
         exit(1);                                                       \
      }                                                                 \
   } while(0)

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
   VkSurfaceKHR surface;

   VkSwapchainKHR swapchain;
   VkExtent2D swapchain_extent;
   VkFormat swapchain_image_format;

   int swapchain_image_count;
   VkImage *swapchain_images;
   VkImageView *swapchain_image_views;

   u64 frame_count;
   vulkan_frame_commands frame_commands[2];

   VkQueue graphics_queue;
   VkQueue present_queue;
} vulkan_context;

static void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout)
{
   VkImageMemoryBarrier2 image_barrier = {0};
   image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
   image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
   image_barrier.oldLayout = old_layout;
   image_barrier.newLayout = new_layout;

   VkImageSubresourceRange subresource_range = {0};
   subresource_range.aspectMask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
      ? VK_IMAGE_ASPECT_DEPTH_BIT
      : VK_IMAGE_ASPECT_COLOR_BIT;
   subresource_range.levelCount = VK_REMAINING_MIP_LEVELS;
   subresource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

   image_barrier.subresourceRange = subresource_range;
   image_barrier.image = image;

   VkDependencyInfo dependency_info = {0};
   dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependency_info.imageMemoryBarrierCount = 1;
   dependency_info.pImageMemoryBarriers = &image_barrier;

   vkCmdPipelineBarrier2(cmd, &dependency_info);
}

int main(void)
{
   vulkan_context vk = {0};

   // Enable instance extensions.
   _Bool instance_extensions_supported = 1;
   const char *enabled_instance_extensions[] = {"VK_EXT_debug_utils", "VK_KHR_surface", "VK_KHR_xlib_surface"};

   u32 instance_extension_count = 0;
   vkEnumerateInstanceExtensionProperties(0, &instance_extension_count, 0);

   VkExtensionProperties *instance_extensions = calloc(instance_extension_count, sizeof(*instance_extensions));
   vkEnumerateInstanceExtensionProperties(0, &instance_extension_count, instance_extensions);

   for(int index = 0; index < countof(enabled_instance_extensions); ++index)
   {
      _Bool found = 0;
      const char *required_name = enabled_instance_extensions[index];
      for(int available_index = 0; available_index < instance_extension_count; ++available_index)
      {
         const char *available_name = instance_extensions[available_index].extensionName;
         if(strcmp(required_name, available_name) == 0)
         {
            printf("  Extension: %s\n", required_name);
            found = 1;
         }
      }
      if(!found)
      {
         instance_extensions_supported = 0;
         break;
      }
   }

   // Enable layers.
   _Bool layers_supported = 1;
   const char *enabled_layers[] = {"VK_LAYER_KHRONOS_validation"};

   u32 layer_count;
   vkEnumerateInstanceLayerProperties(&layer_count, 0);

   VkLayerProperties *available_layers = calloc(layer_count, sizeof(*available_layers));
   vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

   for(int index = 0; index < countof(enabled_layers); ++index)
   {
      _Bool found = 0;

      const char *required_name = enabled_layers[index];
      for(int available_index = 0; available_index < layer_count; ++available_index)
      {
         const char *available_name = available_layers[available_index].layerName;
         if(strcmp(required_name, available_name) == 0)
         {
            printf("  Layer: %s\n", required_name);
            found = 1;
         }
      }

      if(!found)
      {
         layers_supported = 0;
         break;
      }
   }

   // Create instance.
   VkApplicationInfo application_info = {0};
   application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   application_info.pApplicationName = "Vulkan Test Program";
   application_info.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
   application_info.pEngineName = "None";
   application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
   application_info.apiVersion = VK_API_VERSION_1_3;

   VkInstanceCreateInfo create_info = {0};
   create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   create_info.pApplicationInfo = &application_info;
   if(layers_supported)
   {
      create_info.enabledLayerCount = countof(enabled_layers);
      create_info.ppEnabledLayerNames = enabled_layers;
   }
   create_info.enabledExtensionCount = countof(enabled_instance_extensions);
   create_info.ppEnabledExtensionNames = enabled_instance_extensions;

   VK_CHECK(vkCreateInstance(&create_info, 0, &vk.instance));

   // Get physical GPU.
   vk.gpu = VK_NULL_HANDLE;

   uint32_t device_count = 0;
   vkEnumeratePhysicalDevices(vk.instance, &device_count, 0);
   if(device_count == 0)
   {
      fprintf(stderr, "Failed to enumerate physical GPUs.\n");
      exit(1);
   }

   VkPhysicalDevice *available_devices = calloc(device_count, sizeof(*available_devices));
   vkEnumeratePhysicalDevices(vk.instance, &device_count, available_devices);

   for(int device_index = 0; device_index < device_count; ++device_index)
   {
      VkPhysicalDevice device = available_devices[device_index];

      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(device, &properties);

      VkPhysicalDeviceFeatures features;
      vkGetPhysicalDeviceFeatures(device, &features);

      if(features.geometryShader &&
         (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
          properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU))
      {
         vk.gpu = device;
      }
   }
   if(vk.gpu == VK_NULL_HANDLE)
   {
      fprintf(stderr, "Failed to identify a suitable GPU.\n");
      exit(1);
   }

   // Enable device extensions.
   _Bool device_extensions_supported = 1;
   const char *enabled_device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

   u32 device_extension_count = 0;
   vkEnumerateDeviceExtensionProperties(vk.gpu, 0, &device_extension_count, 0);

   VkExtensionProperties *device_extensions = calloc(device_extension_count, sizeof(*device_extensions));
   vkEnumerateDeviceExtensionProperties(vk.gpu, 0, &device_extension_count, device_extensions);

   for(int index = 0; index < countof(enabled_device_extensions); ++index)
   {
      _Bool found = 0;
      const char *required_name = enabled_device_extensions[index];
      for(int available_index = 0; available_index < device_extension_count; ++available_index)
      {
         const char *available_name = device_extensions[available_index].extensionName;
         if(strcmp(required_name, available_name) == 0)
         {
            printf("  Extension: %s\n", required_name);
            found = 1;
         }
      }
      if(!found)
      {
         device_extensions_supported = 0;
         break;
      }
   }

   // Create window.
   SDL_Window *window = SDL_CreateWindow("Vulkan Test Program", 400, 300, SDL_WINDOW_VULKAN);
   if(!window)
   {
      fprintf(stderr, "Failed to create a window.\n");
      exit(1);
   }

   if(!SDL_Vulkan_CreateSurface(window, vk.instance, 0, &vk.surface))
   {
      fprintf(stderr, "Failed to create a window surface.\n");
      exit(1);
   }

   // Initialize queues.
   u32 queue_family_count = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &queue_family_count, 0);

   VkQueueFamilyProperties *queue_families = calloc(queue_family_count, sizeof(*queue_families));
   vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &queue_family_count, queue_families);

   _Bool graphics_queue_found = 0;
   u32 graphics_queue_index;

   _Bool present_queue_found = 0;
   u32 present_queue_index;

   _Bool found_all = 0;
   for(int queue_family_index = 0; queue_family_index < queue_family_count; ++queue_family_index)
   {
      VkQueueFamilyProperties family = queue_families[queue_family_index];
      if(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
         graphics_queue_index = queue_family_index;
         graphics_queue_found = 1;

         VkBool32 present_support = 0;
         vkGetPhysicalDeviceSurfaceSupportKHR(vk.gpu, queue_family_index, vk.surface, &present_support);

         if(present_support)
         {
            present_queue_index = queue_family_index;
            present_queue_found = 1;
         }
      }

      found_all = graphics_queue_found && present_queue_found;
      if(found_all)
      {
         break;
      }
   }
   assert(found_all);

   // Initialize a logical device.
   float queue_priorities[] = {1.0f};
   VkPhysicalDeviceFeatures device_features = {0};

   VkPhysicalDeviceVulkan13Features features13 = {0};
   features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   features13.dynamicRendering = VK_TRUE;
   features13.synchronization2 = VK_TRUE;

   VkPhysicalDeviceFeatures2 features2 = {0};
   features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   features2.pNext = &features13;
   features2.features = device_features;

   int queue_create_info_count = 0;
   VkDeviceQueueCreateInfo queue_create_infos[2] = {0};

   queue_create_infos[queue_create_info_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue_create_infos[queue_create_info_count].queueFamilyIndex = graphics_queue_index;
   queue_create_infos[queue_create_info_count].queueCount = 1;
   queue_create_infos[queue_create_info_count].pQueuePriorities = queue_priorities;
   queue_create_info_count++;

   if(graphics_queue_index != present_queue_index)
   {
      queue_create_infos[queue_create_info_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_infos[queue_create_info_count].queueFamilyIndex = present_queue_index;
      queue_create_infos[queue_create_info_count].queueCount = 1;
      queue_create_infos[queue_create_info_count].pQueuePriorities = queue_priorities;
      queue_create_info_count++;
   }

   VkDeviceCreateInfo device_create_info = {0};
   device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   device_create_info.pNext = &features2;
   device_create_info.pEnabledFeatures = 0;
   device_create_info.pQueueCreateInfos = queue_create_infos;
   device_create_info.queueCreateInfoCount = queue_create_info_count;
   if(layers_supported)
   {
      device_create_info.enabledLayerCount = countof(enabled_layers);
      device_create_info.ppEnabledLayerNames = enabled_layers;
   }
   device_create_info.enabledExtensionCount = countof(enabled_device_extensions);
   device_create_info.ppEnabledExtensionNames = enabled_device_extensions;

   VK_CHECK(vkCreateDevice(vk.gpu, &device_create_info, 0, &vk.device));

   vkGetDeviceQueue(vk.device, graphics_queue_index, 0, &vk.graphics_queue);
   vkGetDeviceQueue(vk.device, present_queue_index, 0, &vk.present_queue);

   // Initialize swapchain.
   VkSurfaceCapabilitiesKHR capabilities = {0};
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.gpu, vk.surface, &capabilities);

   int format_count = 0;
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &format_count, 0);
   VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(*formats));
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &format_count, formats);

   int present_mode_count = 0;
   vkGetPhysicalDeviceSurfacePresentModesKHR(vk.gpu, vk.surface, &present_mode_count, 0);
   VkPresentModeKHR *present_modes = calloc(present_mode_count, sizeof(*present_modes));
   vkGetPhysicalDeviceSurfacePresentModesKHR(vk.gpu, vk.surface, &present_mode_count, present_modes);

   VkSurfaceFormatKHR surface_format = formats[0];
   for(int format_index = 0; format_index < format_count; ++format_index)
   {
      VkSurfaceFormatKHR available_format = formats[format_index];
      if(available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
         available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      {
         surface_format = available_format;
         break;
      }
   }

   VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
   for(int mode_index = 0; mode_index < present_mode_count; ++mode_index)
   {
      VkPresentModeKHR available_mode = present_modes[mode_index];
      if(available_mode == VK_PRESENT_MODE_MAILBOX_KHR)
      {
         present_mode = available_mode;
         break;
      }
   }

   if(capabilities.currentExtent.width != (u32)-1)
   {
      vk.swapchain_extent = capabilities.currentExtent;
   }
   else
   {
      SDL_GetWindowSizeInPixels(window, &vk.swapchain_extent.width, &vk.swapchain_extent.height);
      if(vk.swapchain_extent.width  > capabilities.maxImageExtent.width)  vk.swapchain_extent.width = capabilities.maxImageExtent.width;
      if(vk.swapchain_extent.width  < capabilities.minImageExtent.width)  vk.swapchain_extent.width = capabilities.minImageExtent.width;
      if(vk.swapchain_extent.height > capabilities.maxImageExtent.height) vk.swapchain_extent.height = capabilities.maxImageExtent.height;
      if(vk.swapchain_extent.height < capabilities.minImageExtent.height) vk.swapchain_extent.height = capabilities.minImageExtent.height;
   }

   printf("  Swap extent: %d, %d\n", vk.swapchain_extent.width, vk.swapchain_extent.height);

   vk.swapchain_image_count = capabilities.minImageCount + 1;
   if(capabilities.maxImageCount > 0 && vk.swapchain_image_count > capabilities.maxImageCount)
   {
      vk.swapchain_image_count = capabilities.maxImageCount;
   }

   VkSwapchainCreateInfoKHR swapchain_create_info = {0};
   swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   swapchain_create_info.surface = vk.surface;
   swapchain_create_info.minImageCount = vk.swapchain_image_count;
   swapchain_create_info.imageFormat = surface_format.format;
   swapchain_create_info.imageColorSpace = surface_format.colorSpace;
   swapchain_create_info.imageExtent = vk.swapchain_extent;
   swapchain_create_info.imageArrayLayers = 1;
   // swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
   swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   if(graphics_queue_index != present_queue_index)
   {
      swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      swapchain_create_info.queueFamilyIndexCount = 2;
      swapchain_create_info.pQueueFamilyIndices = (u32[2]){graphics_queue_index, present_queue_index};
   }
   else
   {
      swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
   }
   swapchain_create_info.preTransform = capabilities.currentTransform;
   swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   swapchain_create_info.presentMode = present_mode;
   swapchain_create_info.clipped = VK_TRUE;
   swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

   VK_CHECK(vkCreateSwapchainKHR(vk.device, &swapchain_create_info, 0, &vk.swapchain));

   vk.swapchain_image_format = surface_format.format;

   vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, 0);
   vk.swapchain_images = calloc(vk.swapchain_image_count, sizeof(*vk.swapchain_images));
   vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images);

   vk.swapchain_image_views = calloc(vk.swapchain_image_count, sizeof(*vk.swapchain_image_views));
   for(int image_index = 0; image_index < vk.swapchain_image_count; ++image_index)
   {
      VkImageViewCreateInfo info = {0};
      info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      info.image = vk.swapchain_images[image_index];
      info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      info.format = vk.swapchain_image_format;
      info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      info.subresourceRange.baseMipLevel = 0;
      info.subresourceRange.levelCount = 1;
      info.subresourceRange.baseArrayLayer = 0;
      info.subresourceRange.layerCount = 1;

      VK_CHECK(vkCreateImageView(vk.device, &info, 0, &vk.swapchain_image_views[image_index]));
   }

   // Initialize commands.
   VkCommandPoolCreateInfo command_pool_info = {0};
   command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   command_pool_info.queueFamilyIndex = graphics_queue_index;

   for(int frame_index = 0; frame_index < countof(vk.frame_commands); ++frame_index)
   {
      vkCreateCommandPool(vk.device, &command_pool_info, 0, &vk.frame_commands[frame_index].pool);

      VkCommandBufferAllocateInfo allocate_info = {0};
      allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocate_info.commandPool = vk.frame_commands[frame_index].pool;
      allocate_info.commandBufferCount = 1;
      allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      VK_CHECK(vkAllocateCommandBuffers(vk.device, &allocate_info, &vk.frame_commands[frame_index].commands));
   }

   // Initialize synchronization.
   VkFenceCreateInfo fence_info = {0};
   fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   VkSemaphoreCreateInfo semaphore_info = {0};
   semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   for(int frame_index = 0; frame_index < countof(vk.frame_commands); ++frame_index)
   {
      VK_CHECK(vkCreateSemaphore(vk.device, &semaphore_info, 0, &vk.frame_commands[frame_index].swapchain_semaphore));
      VK_CHECK(vkCreateSemaphore(vk.device, &semaphore_info, 0, &vk.frame_commands[frame_index].render_semaphore));
      VK_CHECK(vkCreateFence(vk.device, &fence_info, 0, &vk.frame_commands[frame_index].render_fence));
   }

   // Render loop.
   _Bool quit = 0;
   while(!quit)
   {
      SDL_Event event;
      while(SDL_PollEvent(&event))
      {
         if(event.type == SDL_EVENT_QUIT) quit = 1;
         if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) quit = 1;
      }

      vulkan_frame_commands *frame = vk.frame_commands + (vk.frame_count % countof(vk.frame_commands));

      VK_CHECK(vkWaitForFences(vk.device, 1, &frame->render_fence, 1, 1000000000));
      VK_CHECK(vkResetFences(vk.device, 1, &frame->render_fence));

      u32 swapchain_image_index;
      VK_CHECK(vkAcquireNextImageKHR(vk.device, vk.swapchain, 1000000000, frame->swapchain_semaphore, 0, &swapchain_image_index));

      VkCommandBuffer cmd = frame->commands;
      VK_CHECK(vkResetCommandBuffer(cmd, 0));

      VkCommandBufferBeginInfo begin_info = {0};
      begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

      transition_image(cmd, vk.swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

      VkImageSubresourceRange clear_range = {0};
      clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      clear_range.levelCount = VK_REMAINING_MIP_LEVELS;
      clear_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

      VkClearColorValue color = {0, 0, 1, 1};
      vkCmdClearColorImage(cmd, vk.swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_GENERAL, &color, 1, &clear_range);
      transition_image(cmd, vk.swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

      VK_CHECK(vkEndCommandBuffer(cmd));

      VkCommandBufferSubmitInfo cmd_info = {0};
      cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
      cmd_info.commandBuffer = cmd;

      VkSemaphoreSubmitInfo wait_info = {0};
      wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      wait_info.semaphore = frame->swapchain_semaphore;
      wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
      wait_info.value = 1;

      VkSemaphoreSubmitInfo signal_info = {0};
      signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      signal_info.semaphore = frame->render_semaphore;
      signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
      signal_info.value = 1;

      VkSubmitInfo2 submit_info = {0};
      submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
      submit_info.waitSemaphoreInfoCount = 1;
      submit_info.pWaitSemaphoreInfos = &wait_info;
      submit_info.signalSemaphoreInfoCount = 1;
      submit_info.pSignalSemaphoreInfos = &signal_info;
      submit_info.commandBufferInfoCount = 1;
      submit_info.pCommandBufferInfos = &cmd_info;

      VK_CHECK(vkQueueSubmit2(vk.graphics_queue, 1, &submit_info, frame->render_fence));

      VkPresentInfoKHR present_info = {};
      present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      present_info.pSwapchains = &vk.swapchain;
      present_info.swapchainCount = 1;
      present_info.pWaitSemaphores = &frame->render_semaphore;
      present_info.waitSemaphoreCount = 1;
      present_info.pImageIndices = &swapchain_image_index;

      VK_CHECK(vkQueuePresentKHR(vk.graphics_queue, &present_info));

      vk.frame_count++;
   };

   // Clean up.
   vkDeviceWaitIdle(vk.device);
   for(int frame_index = 0; frame_index < countof(vk.frame_commands); ++frame_index)
   {
      vkDestroyFence(vk.device, vk.frame_commands[frame_index].render_fence, 0);
      vkDestroySemaphore(vk.device, vk.frame_commands[frame_index].render_semaphore, 0);
      vkDestroySemaphore(vk.device, vk.frame_commands[frame_index].swapchain_semaphore, 0);
      vkDestroyCommandPool(vk.device, vk.frame_commands[frame_index].pool, 0);
   }
   vkDestroySwapchainKHR(vk.device, vk.swapchain, 0);
   for(int image_index = 0; image_index < vk.swapchain_image_count; ++image_index)
   {
      vkDestroyImageView(vk.device, vk.swapchain_image_views[image_index], 0);
   }
   vkDestroySurfaceKHR(vk.instance, vk.surface, 0);
   vkDestroyDevice(vk.device, 0);

   return(0);
}
