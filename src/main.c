#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "vk_mem_alloc.h"
#include "window_creation.h"

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
         __builtin_trap();                                              \
      }                                                                 \
   } while(0)

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
            // printf("  Extension: %s\n", required_name);
            found = 1;
         }
      }
      if(!found)
      {
         instance_extensions_supported = 0;
         break;
      }
   }
   if(!instance_extensions_supported)
   {
      fprintf(stderr, "Error: Not all requested instance extensions supported.");
      exit(1);
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
            // printf("  Layer: %s\n", required_name);
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
            // printf("  Extension: %s\n", required_name);
            found = 1;
         }
      }
      if(!found)
      {
         device_extensions_supported = 0;
         break;
      }
   }

   if(!device_extensions_supported)
   {
      fprintf(stderr, "Error: Not all requested device xtensions supported.");
      exit(1);
   }

   // Create window.
   window *wnd = create_window("Vulkan Test Program", 400, 300);
   if(!create_window_surface(wnd, vk.instance, &vk.surface))
   {
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

   VkPhysicalDeviceVulkan12Features features12 = {0};
   features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   features12.bufferDeviceAddress = VK_TRUE;

   VkPhysicalDeviceVulkan13Features features13 = {0};
   features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   features13.pNext = &features12;
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

   u32 format_count = 0;
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &format_count, 0);
   VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(*formats));
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &format_count, formats);

   u32 present_mode_count = 0;
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
      get_window_dimensions(wnd, (int *)&vk.swapchain_extent.width, (int *)&vk.swapchain_extent.height);

      if(vk.swapchain_extent.width > capabilities.maxImageExtent.width) vk.swapchain_extent.width = capabilities.maxImageExtent.width;
      if(vk.swapchain_extent.width < capabilities.minImageExtent.width) vk.swapchain_extent.width = capabilities.minImageExtent.width;

      if(vk.swapchain_extent.height > capabilities.maxImageExtent.height) vk.swapchain_extent.height = capabilities.maxImageExtent.height;
      if(vk.swapchain_extent.height < capabilities.minImageExtent.height) vk.swapchain_extent.height = capabilities.minImageExtent.height;
   }

   // printf("  Swap extent: %d, %d\n", vk.swapchain_extent.width, vk.swapchain_extent.height);

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

   // Initialize allocator.
   VmaAllocatorCreateInfo allocator_info = {0};
   allocator_info.physicalDevice = vk.gpu;
   allocator_info.device = vk.device;
   allocator_info.instance = vk.instance;
   allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
   VK_CHECK(vmaCreateAllocator(&allocator_info, &vk.allocator));

   // Initialize draw image.
   VkExtent3D draw_image_extent = {vk.swapchain_extent.width, vk.swapchain_extent.height, 1};
   vk.draw_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
   vk.draw_image.extent = draw_image_extent;

   VkImageUsageFlags draw_image_usages = 0;
   draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
   draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   VkImageCreateInfo rimg_info = {0};
   rimg_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   rimg_info.imageType = VK_IMAGE_TYPE_2D;
   rimg_info.format = vk.draw_image.format;
   rimg_info.extent = vk.draw_image.extent;
   rimg_info.mipLevels = 1;
   rimg_info.arrayLayers = 1;
   rimg_info.samples = VK_SAMPLE_COUNT_1_BIT;
   rimg_info.tiling = VK_IMAGE_TILING_OPTIMAL;
   rimg_info.usage = draw_image_usages;

   VmaAllocationCreateInfo rimg_alloc_info = {0};
   rimg_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
   rimg_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

   VK_CHECK(vmaCreateImage(vk.allocator, &rimg_info, &rimg_alloc_info, &vk.draw_image.image, &vk.draw_image.allocation, 0));

   VkImageViewCreateInfo rview_info = {0};
   rview_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   rview_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
   rview_info.image = vk.draw_image.image;
   rview_info.format = vk.draw_image.format;
   rview_info.subresourceRange.levelCount = 1;
   rview_info.subresourceRange.layerCount = 1;
   rview_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

   VK_CHECK(vkCreateImageView(vk.device, &rview_info, 0, &vk.draw_image.view));

   // Initialize descriptors.
   VkDescriptorSetLayoutBinding binding = {0};
   binding.binding = 0;
   binding.descriptorCount = 1;
   binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

   VkDescriptorSetLayoutCreateInfo layout_info = {0};
   layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layout_info.pNext = 0;
   layout_info.pBindings = &binding;
   layout_info.bindingCount = 1;
   layout_info.flags = 0;

   VkDescriptorSetLayout layout;
   VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &layout_info, 0, &layout));

   u32 max_sets = 10;
   VkDescriptorPoolSize pool_size = {0};
   pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   pool_size.descriptorCount = 1 * max_sets;

   VkDescriptorPoolCreateInfo pool_info = {0};
   pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool_info.maxSets = max_sets;
   pool_info.poolSizeCount = 1;
   pool_info.pPoolSizes = &pool_size;

   VkDescriptorPool pool;
   vkCreateDescriptorPool(vk.device, &pool_info, 0, &pool);

   VkDescriptorSetAllocateInfo allocation_info = {0};
   allocation_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocation_info.descriptorPool = pool;
   allocation_info.descriptorSetCount = 1;
   allocation_info.pSetLayouts = &layout;

   VkDescriptorSet descriptor_set;
   VK_CHECK(vkAllocateDescriptorSets(vk.device, &allocation_info, &descriptor_set));

   VkDescriptorImageInfo image_info = {0};
   image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
   image_info.imageView = vk.draw_image.view;

   VkWriteDescriptorSet draw_image_write = {0};
   draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   draw_image_write.dstBinding = 0;
   draw_image_write.dstSet = descriptor_set;
   draw_image_write.descriptorCount = 1;
   draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   draw_image_write.pImageInfo = &image_info;

   vkUpdateDescriptorSets(vk.device, 1, &draw_image_write, 0, 0);

   // Load shaders.
   FILE *shader_file = fopen("shaders/gradient.comp.spv", "rb");
   assert(shader_file);

   fseek(shader_file, 0, SEEK_END);
   size_t shader_file_size = ftell(shader_file);
   fseek(shader_file, 0, SEEK_SET);

   u32 *shader_code = malloc(shader_file_size);
   assert(shader_code);

   fread(shader_code, shader_file_size, 1, shader_file);
   fclose(shader_file);

   VkShaderModuleCreateInfo shader_create_info = {0};
   shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   shader_create_info.codeSize = shader_file_size;
   shader_create_info.pCode = shader_code;

   VkShaderModule shader_module;
   VK_CHECK(vkCreateShaderModule(vk.device, &shader_create_info, 0, &shader_module));

   VkPipelineLayoutCreateInfo compute_layout = {0};
   compute_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   compute_layout.pSetLayouts = &layout;
   compute_layout.setLayoutCount = 1;

   VkPipelineLayout gradient_pipeline_layout;
   VK_CHECK(vkCreatePipelineLayout(vk.device, &compute_layout, 0, &gradient_pipeline_layout));

   VkPipelineShaderStageCreateInfo stage_create_info = {0};
   stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
   stage_create_info.module = shader_module;
   stage_create_info.pName = "main";

   VkComputePipelineCreateInfo compute_pipeline_create_info = {0};
   compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   compute_pipeline_create_info.layout = gradient_pipeline_layout;
   compute_pipeline_create_info.stage = stage_create_info;

   VkPipeline gradient_pipeline;
   VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, 0, &gradient_pipeline));

   // Render loop.
   while(!window_should_close())
   {
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

      transition_image(cmd, vk.draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

      VkImageSubresourceRange clear_range = {0};
      clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      clear_range.levelCount = VK_REMAINING_MIP_LEVELS;
      clear_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

      VkClearColorValue color = {{0, 0, 1, 1}};
      vkCmdClearColorImage(cmd, vk.draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &clear_range);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradient_pipeline);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradient_pipeline_layout, 0, 1, &descriptor_set, 0, 0);
      vkCmdDispatch(cmd, ceilf(vk.draw_image.extent.width/16.0f), ceilf(vk.draw_image.extent.height/16.0f), 1);

      transition_image(cmd, vk.draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      transition_image(cmd, vk.swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      // Copy image to swapchain.
      VkImageBlit2 blit_region = {0};
      blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
      blit_region.srcOffsets[1].x = vk.draw_image.extent.width;
      blit_region.srcOffsets[1].y = vk.draw_image.extent.height;
      blit_region.srcOffsets[1].z = 1;
      blit_region.dstOffsets[1].x = vk.swapchain_extent.width;
      blit_region.dstOffsets[1].y = vk.swapchain_extent.height;
      blit_region.dstOffsets[1].z = 1;
      blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit_region.srcSubresource.baseArrayLayer = 0;
      blit_region.srcSubresource.layerCount = 1;
      blit_region.srcSubresource.mipLevel = 0;
      blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit_region.dstSubresource.baseArrayLayer = 0;
      blit_region.dstSubresource.layerCount = 1;
      blit_region.dstSubresource.mipLevel = 0;

      VkBlitImageInfo2 blit_info = {0};
      blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
      blit_info.dstImage = vk.swapchain_images[swapchain_image_index];
      blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      blit_info.srcImage = vk.draw_image.image;
      blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      blit_info.filter = VK_FILTER_LINEAR;
      blit_info.regionCount = 1;
      blit_info.pRegions = &blit_region;

      vkCmdBlitImage2(cmd, &blit_info);

      transition_image(cmd, vk.swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

      VK_CHECK(vkEndCommandBuffer(cmd));

      VkCommandBufferSubmitInfo cmd_info = {0};
      cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
      cmd_info.commandBuffer = cmd;

      VkSubmitInfo2 submit_info = {0};
      submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
      submit_info.waitSemaphoreInfoCount = 1;
      submit_info.pWaitSemaphoreInfos = &(VkSemaphoreSubmitInfo){
         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
         .semaphore = frame->swapchain_semaphore,
         .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
         .value = 1,
      };
      submit_info.signalSemaphoreInfoCount = 1;
      submit_info.pSignalSemaphoreInfos = &(VkSemaphoreSubmitInfo){
         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
         .semaphore = frame->render_semaphore,
         .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
         .value = 1,
      };
      submit_info.commandBufferInfoCount = 1;
      submit_info.pCommandBufferInfos = &cmd_info;

      VK_CHECK(vkQueueSubmit2(vk.graphics_queue, 1, &submit_info, frame->render_fence));

      VkPresentInfoKHR present_info = {0};
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

   vkDestroyShaderModule(vk.device, shader_module, 0);
   vkDestroyPipelineLayout(vk.device, gradient_pipeline_layout, 0);
   vkDestroyPipeline(vk.device, gradient_pipeline, 0);

   vkDestroyDescriptorPool(vk.device, pool, 0);
   vkDestroyDescriptorSetLayout(vk.device, layout, 0);

   vkDestroyImageView(vk.device, vk.draw_image.view, 0);
   vmaDestroyImage(vk.allocator, vk.draw_image.image, vk.draw_image.allocation);
   vmaDestroyAllocator(vk.allocator);
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
