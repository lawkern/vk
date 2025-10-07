#include "vk.h"
#include "window_creation.h"

static void load_shader_module(VkShaderModule *result, VkDevice device, memory_arena arena, char *path)
{
   FILE *shader_file = fopen(path, "rb");
   assert(shader_file);

   fseek(shader_file, 0, SEEK_END);
   size_t shader_file_size = ftell(shader_file);
   fseek(shader_file, 0, SEEK_SET);

   memory_index count = shader_file_size / sizeof(u32);
   u32 *shader_code = allocate(&arena, count, u32);
   assert(shader_code);

   fread(shader_code, shader_file_size, 1, shader_file);
   fclose(shader_file);

   VkShaderModuleCreateInfo shader_module_info = {0};
   shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   shader_module_info.codeSize = shader_file_size;
   shader_module_info.pCode = shader_code;

   VK_CHECK(vkCreateShaderModule(device, &shader_module_info, 0, result));
}

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

static void copy_image(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D src_size, VkExtent2D dst_size)
{
   VkImageBlit2 blit_region = {0};
   blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
   blit_region.srcOffsets[1].x = src_size.width;
   blit_region.srcOffsets[1].y = src_size.height;
   blit_region.srcOffsets[1].z = 1;
   blit_region.dstOffsets[1].x = dst_size.width;
   blit_region.dstOffsets[1].y = dst_size.height;
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
   blit_info.dstImage = dst;
   blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   blit_info.srcImage = src;
   blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   blit_info.filter = VK_FILTER_LINEAR;
   blit_info.regionCount = 1;
   blit_info.pRegions = &blit_region;

   vkCmdBlitImage2(cmd, &blit_info);
}

static void initialize_pipeline_config(vulkan_pipeline_configuration *config)
{
   config->input_assembly         = (VkPipelineInputAssemblyStateCreateInfo){.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
   config->rasterizer             = (VkPipelineRasterizationStateCreateInfo){.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
   config->color_blend_attachment = (VkPipelineColorBlendAttachmentState){0};
   config->multisampling          = (VkPipelineMultisampleStateCreateInfo){.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
   config->layout                 = (VkPipelineLayout){0};
   config->depth_stencil          = (VkPipelineDepthStencilStateCreateInfo){.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
   config->rendering_info         = (VkPipelineRenderingCreateInfo){.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

   for(int shader_index = 0; shader_index < countof(config->shader_stages); ++shader_index)
   {
      config->shader_stages[shader_index] = (VkPipelineShaderStageCreateInfo){0};
   }
}

static VkPipeline create_pipeline(vulkan_pipeline_configuration *config, VkDevice device)
{
   VkPipelineViewportStateCreateInfo viewport_state = {0};
   viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewport_state.viewportCount = 1;
   viewport_state.scissorCount = 1;

   VkPipelineColorBlendStateCreateInfo color_blending = {0};
   color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   color_blending.logicOpEnable = VK_FALSE;
   color_blending.logicOp = VK_LOGIC_OP_COPY;
   color_blending.attachmentCount = 1;
   color_blending.pAttachments = &config->color_blend_attachment;

   VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
   vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

   VkGraphicsPipelineCreateInfo pipeline_info = {0};
   pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipeline_info.pNext = &config->rendering_info;

   pipeline_info.stageCount = countof(config->shader_stages);
   pipeline_info.pStages = config->shader_stages;
   pipeline_info.pVertexInputState = &vertex_input_info;
   pipeline_info.pInputAssemblyState = &config->input_assembly;
   pipeline_info.pViewportState = &viewport_state;
   pipeline_info.pRasterizationState = &config->rasterizer;
   pipeline_info.pMultisampleState = &config->multisampling;
   pipeline_info.pColorBlendState = &color_blending;
   pipeline_info.pDepthStencilState = &config->depth_stencil;
   pipeline_info.layout = config->layout;

   VkDynamicState dynamic_states[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
   };
   VkPipelineDynamicStateCreateInfo dynamic_info = {0};
   dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   dynamic_info.pDynamicStates = dynamic_states;
   dynamic_info.dynamicStateCount = countof(dynamic_states);

   pipeline_info.pDynamicState = &dynamic_info;

   VkPipeline result;
   VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, 0, &result));

   return(result);
}

static void draw_background(vulkan_context *vk, VkDescriptorSet *descriptor_set, VkCommandBuffer cmd)
{
   VkImageSubresourceRange clear_range = {0};
   clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   clear_range.levelCount = VK_REMAINING_MIP_LEVELS;
   clear_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

   VkClearColorValue color = {{0, 0, 1, 1}};
   vkCmdClearColorImage(cmd, vk->draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &clear_range);

   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk->background_effect.pipeline);
   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk->background_effect.layout, 0, 1, descriptor_set, 0, 0);
   vkCmdPushConstants(cmd, vk->background_effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vk->background_effect.constants), &vk->background_effect.constants);
   vkCmdDispatch(cmd, ceilf(vk->draw_extent.width/16.0f), ceilf(vk->draw_extent.height/16.0f), 1);
}

static void draw_geometry(vulkan_context *vk, VkCommandBuffer cmd, VkDeviceAddress vertex_buffer_address, VkBuffer index_buffer)
{
   VkRenderingAttachmentInfo color_attachment_info = {0};
   color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
   color_attachment_info.imageView = vk->draw_image.view;
   color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   color_attachment_info.resolveMode = 0;
   color_attachment_info.resolveImageView = 0;
   color_attachment_info.resolveImageLayout = 0;
   color_attachment_info.loadOp = 0;
   color_attachment_info.storeOp = 0;
   color_attachment_info.clearValue = (VkClearValue){0};

   VkRenderingInfo rendering_info = {0};
   rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
   rendering_info.renderArea = (VkRect2D){.extent = vk->draw_extent};
   rendering_info.layerCount = 1;
   rendering_info.viewMask = 0;
   rendering_info.colorAttachmentCount = 1;
   rendering_info.pColorAttachments = &color_attachment_info;
   rendering_info.pDepthAttachment = 0;
   rendering_info.pStencilAttachment = 0;

   vkCmdBeginRendering(cmd, &rendering_info);
   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->triangle_pipeline);

   VkViewport viewport = {0};
   viewport.x = 0;
   viewport.y = 0;
   viewport.width = vk->draw_extent.width;
   viewport.height = vk->draw_extent.height;
   viewport.minDepth = 0.f;
   viewport.maxDepth = 1.f;

   vkCmdSetViewport(cmd, 0, 1, &viewport);

   VkRect2D scissor = {0};
   scissor.offset.x = 0;
   scissor.offset.y = 0;
   scissor.extent.width = vk->draw_extent.width;
   scissor.extent.height = vk->draw_extent.height;

   vkCmdSetScissor(cmd, 0, 1, &scissor);

   vkCmdDraw(cmd, 3, 1, 0, 0);

   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->mesh_pipeline);

   mesh_push_constants push_constants = {0};
   push_constants.world_matrix = (mat4){
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
   };
   push_constants.vertex_buffer = vertex_buffer_address;

   vkCmdPushConstants(cmd, vk->mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &push_constants);
   vkCmdBindIndexBuffer(cmd, index_buffer, 0, VK_INDEX_TYPE_UINT32);

   vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

   vkCmdEndRendering(cmd);
}

static vulkan_buffer create_buffer(VmaAllocator allocator, memory_index size, VkBufferUsageFlags buffer_usage, VmaMemoryUsage memory_usage)
{
   VkBufferCreateInfo buffer_info = {0};
   buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   buffer_info.size = size;
   buffer_info.usage = buffer_usage;

   VmaAllocationCreateInfo alloc_info = {0};
   alloc_info.usage = memory_usage;
   alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

   vulkan_buffer result = {0};
   VK_CHECK(vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &result.buffer, &result.allocation, &result.info));

   return(result);
}

static void immediate_prepare(vulkan_context *vk)
{
   VkCommandBuffer cmd = vk->immediate_command_buffer;

   VK_CHECK(vkResetFences(vk->device, 1, &vk->immediate_fence));
   VK_CHECK(vkResetCommandBuffer(cmd, 0));

   VkCommandBufferBeginInfo begin_info = {0};
   begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
}

static void immediate_submit(vulkan_context *vk)
{
   VkCommandBuffer cmd = vk->immediate_command_buffer;

   VK_CHECK(vkEndCommandBuffer(cmd));

   VkCommandBufferSubmitInfo cmd_info = {0};
   cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
   cmd_info.commandBuffer = cmd;

   VkSubmitInfo2 submit_info = {0};
   submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
   submit_info.waitSemaphoreInfoCount = 0;
   submit_info.pWaitSemaphoreInfos = 0;
   submit_info.signalSemaphoreInfoCount = 0;
   submit_info.pSignalSemaphoreInfos = 0;
   submit_info.commandBufferInfoCount = 1;
   submit_info.pCommandBufferInfos = &cmd_info;

   VK_CHECK(vkQueueSubmit2(vk->graphics_queue, 1, &submit_info, vk->immediate_fence));
   VK_CHECK(vkWaitForFences(vk->device, 1, &vk->immediate_fence, 0, UINT64_MAX));
}

static vulkan_mesh push_mesh(vulkan_context *vk, vertex *vertices, int vertex_count, u32 *indices, int index_count)
{
   vulkan_mesh result = {0};

   memory_index vertex_buffer_size = vertex_count * sizeof(*vertices);
   memory_index index_buffer_size = index_count * sizeof(*indices);

   result.vertices = create_buffer(vk->allocator, vertex_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
   result.indices = create_buffer(vk->allocator, index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

   VkBufferDeviceAddressInfo device_address_info = {0};
   device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
   device_address_info.buffer = result.vertices.buffer;

   result.vertex_address = vkGetBufferDeviceAddress(vk->device, &device_address_info);

   vulkan_buffer staging_buffer = create_buffer(vk->allocator, vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
   VmaAllocationInfo staging_allocation_info;
   vmaGetAllocationInfo(vk->allocator, staging_buffer.allocation, &staging_allocation_info);

   void *staging_data = staging_allocation_info.pMappedData;
   memcpy(staging_data, vertices, vertex_buffer_size);
   memcpy((char *)staging_data + vertex_buffer_size, indices, index_buffer_size);

   immediate_prepare(vk);
   {
      VkCommandBuffer cmd = vk->immediate_command_buffer;

      VkBufferCopy vertex_copy = {0};
      vertex_copy.dstOffset = 0;
      vertex_copy.srcOffset = 0;
      vertex_copy.size = vertex_buffer_size;

      vkCmdCopyBuffer(cmd, staging_buffer.buffer, result.vertices.buffer, 1, &vertex_copy);

      VkBufferCopy index_copy = {0};
      index_copy.dstOffset = 0;
      index_copy.srcOffset = vertex_buffer_size;
      index_copy.size = index_buffer_size;

      vkCmdCopyBuffer(cmd, staging_buffer.buffer, result.indices.buffer, 1, &index_copy);
   }
   immediate_submit(vk);

   vmaDestroyBuffer(vk->allocator, staging_buffer.buffer, staging_buffer.allocation);



   return(result);
}

int main(void)
{
   memory_index arena_size = 1024*1024;
   memory_arena arena = {0};
   arena.begin = malloc(arena_size);
   arena.end = arena.begin + arena_size;

   memory_index scratch_size = 1024*1024;
   memory_arena scratch = {0};
   scratch.begin = malloc(scratch_size);
   scratch.end = scratch.begin + scratch_size;

   // Enable instance extensions.
   u32 instance_extension_count = 0;
   vkEnumerateInstanceExtensionProperties(0, &instance_extension_count, 0);

   VkExtensionProperties *instance_extensions = allocate(&arena, instance_extension_count, VkExtensionProperties);
   vkEnumerateInstanceExtensionProperties(0, &instance_extension_count, instance_extensions);

   const char *required_instance_extensions[] = {
      "VK_EXT_debug_utils",
      "VK_KHR_surface",
      "VK_KHR_xlib_surface",
   };
   for(int required_index = 0; required_index < countof(required_instance_extensions); ++required_index)
   {
      b32 found = 0;
      const char *required_name = required_instance_extensions[required_index];
      for(int available_index = 0; available_index < instance_extension_count; ++available_index)
      {
         const char *available_name = instance_extensions[available_index].extensionName;
         if(strcmp(required_name, available_name) == 0)
         {
            found = 1;
            break;
         }
      }
      if(!found)
      {
         fprintf(stderr, "Error: Requested instance extension %s not available.\n", required_name);
         exit(1);
      }
   }

   // Enable layers.
   u32 layer_count;
   vkEnumerateInstanceLayerProperties(&layer_count, 0);

   VkLayerProperties *available_layers = allocate(&arena, layer_count, VkLayerProperties);
   vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

   const char *required_layers[] = {
      "VK_LAYER_KHRONOS_validation",
   };
   for(int index = 0; index < countof(required_layers); ++index)
   {
      b32 found = 0;
      const char *required_name = required_layers[index];
      for(int available_index = 0; available_index < layer_count; ++available_index)
      {
         const char *available_name = available_layers[available_index].layerName;
         if(strcmp(required_name, available_name) == 0)
         {
            found = 1;
            break;
         }
      }
      if(!found)
      {
         fprintf(stderr, "Error: Requested layer %s not available.\n", required_name);
         exit(1);
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

   VkInstanceCreateInfo instance_create_info = {0};
   instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   instance_create_info.pApplicationInfo = &application_info;
   instance_create_info.enabledLayerCount = countof(required_layers);
   instance_create_info.ppEnabledLayerNames = required_layers;
   instance_create_info.enabledExtensionCount = countof(required_instance_extensions);
   instance_create_info.ppEnabledExtensionNames = required_instance_extensions;

   vulkan_context vk = {0};
   VK_CHECK(vkCreateInstance(&instance_create_info, 0, &vk.instance));

   // Get physical GPU.
   uint32_t gpu_count = 0;
   vkEnumeratePhysicalDevices(vk.instance, &gpu_count, 0);
   if(gpu_count == 0)
   {
      fprintf(stderr, "Failed to enumerate physical GPUs.\n");
      exit(1);
   }

   VkPhysicalDevice *available_gpus = allocate(&arena, gpu_count, VkPhysicalDevice);
   vkEnumeratePhysicalDevices(vk.instance, &gpu_count, available_gpus);

   for(int gpu_index = 0; gpu_index < gpu_count; ++gpu_index)
   {
      VkPhysicalDevice gpu = available_gpus[gpu_index];

      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(gpu, &properties);

      VkPhysicalDeviceFeatures features;
      vkGetPhysicalDeviceFeatures(gpu, &features);

      if(features.geometryShader &&
         (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
          properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU))
      {
         vk.gpu = gpu;
         break;
      }
   }
   if(!vk.gpu)
   {
      fprintf(stderr, "Failed to identify a suitable GPU.\n");
      exit(1);
   }

   // Enable device extensions.
   u32 device_extension_count = 0;
   vkEnumerateDeviceExtensionProperties(vk.gpu, 0, &device_extension_count, 0);

   VkExtensionProperties *device_extensions = allocate(&arena, device_extension_count, VkExtensionProperties);
   vkEnumerateDeviceExtensionProperties(vk.gpu, 0, &device_extension_count, device_extensions);

   const char *required_device_extensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
   };
   for(int required_index = 0; required_index < countof(required_device_extensions); ++required_index)
   {
      b32 found = 0;
      const char *required_name = required_device_extensions[required_index];
      for(int available_index = 0; available_index < device_extension_count; ++available_index)
      {
         const char *available_name = device_extensions[available_index].extensionName;
         if(strcmp(required_name, available_name) == 0)
         {
            found = 1;
            break;
         }
      }
      if(!found)
      {
         fprintf(stderr, "Error: Requested device extension %s not available.\n", required_name);
         exit(1);
      }
   }

   // Create window and surface.
   if(!create_window(&vk, "Vulkan Test Program", 400*2, 300*2))
   {
      exit(1);
   }

   // Initialize queues.
   u32 queue_family_count = 0;
   vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &queue_family_count, 0);

   VkQueueFamilyProperties *queue_families = allocate(&arena, queue_family_count, VkQueueFamilyProperties);
   vkGetPhysicalDeviceQueueFamilyProperties(vk.gpu, &queue_family_count, queue_families);

   b32 graphics_queue_found = 0;
   u32 graphics_queue_index;

   b32 present_queue_found = 0;
   u32 present_queue_index;

   b32 found_all = 0;
   for(int queue_family_index = 0; queue_family_index < queue_family_count; ++queue_family_index)
   {
      VkQueueFamilyProperties family = queue_families[queue_family_index];
      if(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
         graphics_queue_index = queue_family_index;
         graphics_queue_found = 1;

         VkBool32 present_support;
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
   device_create_info.enabledLayerCount = countof(required_layers);
   device_create_info.ppEnabledLayerNames = required_layers;
   device_create_info.enabledExtensionCount = countof(required_device_extensions);
   device_create_info.ppEnabledExtensionNames = required_device_extensions;

   VK_CHECK(vkCreateDevice(vk.gpu, &device_create_info, 0, &vk.device));

   vkGetDeviceQueue(vk.device, graphics_queue_index, 0, &vk.graphics_queue);
   vkGetDeviceQueue(vk.device, present_queue_index, 0, &vk.present_queue);

   // Initialize swapchain.
   VkSurfaceCapabilitiesKHR capabilities = {0};
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.gpu, vk.surface, &capabilities);

   u32 format_count = 0;
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &format_count, 0);
   VkSurfaceFormatKHR *formats = allocate(&arena, format_count, VkSurfaceFormatKHR);
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk.gpu, vk.surface, &format_count, formats);

   u32 present_mode_count = 0;
   vkGetPhysicalDeviceSurfacePresentModesKHR(vk.gpu, vk.surface, &present_mode_count, 0);
   VkPresentModeKHR *present_modes = allocate(&arena, present_mode_count, VkPresentModeKHR);
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
      get_window_dimensions(&vk, (int *)&vk.swapchain_extent.width, (int *)&vk.swapchain_extent.height);

      if(vk.swapchain_extent.width > capabilities.maxImageExtent.width) vk.swapchain_extent.width = capabilities.maxImageExtent.width;
      if(vk.swapchain_extent.width < capabilities.minImageExtent.width) vk.swapchain_extent.width = capabilities.minImageExtent.width;

      if(vk.swapchain_extent.height > capabilities.maxImageExtent.height) vk.swapchain_extent.height = capabilities.maxImageExtent.height;
      if(vk.swapchain_extent.height < capabilities.minImageExtent.height) vk.swapchain_extent.height = capabilities.minImageExtent.height;
   }

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
   vk.swapchain_images = allocate(&arena, vk.swapchain_image_count, VkImage);
   vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images);

   vk.swapchain_image_views = allocate(&arena, vk.swapchain_image_count, VkImageView);
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

   VkImageCreateInfo image_create_info = {0};
   image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image_create_info.imageType = VK_IMAGE_TYPE_2D;
   image_create_info.format = vk.draw_image.format;
   image_create_info.extent = vk.draw_image.extent;
   image_create_info.mipLevels = 1;
   image_create_info.arrayLayers = 1;
   image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
   image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
   image_create_info.usage = draw_image_usages;

   VmaAllocationCreateInfo image_alloc_info = {0};
   image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
   image_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

   VK_CHECK(vmaCreateImage(vk.allocator, &image_create_info, &image_alloc_info, &vk.draw_image.image, &vk.draw_image.allocation, 0));

   VkImageViewCreateInfo image_view_info = {0};
   image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
   image_view_info.image = vk.draw_image.image;
   image_view_info.format = vk.draw_image.format;
   image_view_info.subresourceRange.levelCount = 1;
   image_view_info.subresourceRange.layerCount = 1;
   image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

   VK_CHECK(vkCreateImageView(vk.device, &image_view_info, 0, &vk.draw_image.view));

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

   // Initialize compute pipeline.
   VkShaderModule compute_shader_module;
   load_shader_module(&compute_shader_module, vk.device, arena, "gradient_color.comp.spv");

   VkPipelineLayoutCreateInfo compute_layout_info = {0};
   compute_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   compute_layout_info.pSetLayouts = &layout;
   compute_layout_info.setLayoutCount = 1;

   VkPushConstantRange push_constant_range = {0};
   push_constant_range.offset = 0;
   push_constant_range.size = sizeof(compute_push_constants);
   push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

   compute_layout_info.pPushConstantRanges = &push_constant_range;
   compute_layout_info.pushConstantRangeCount = 1;

   VkPipelineLayout compute_pipeline_layout;
   VK_CHECK(vkCreatePipelineLayout(vk.device, &compute_layout_info, 0, &compute_pipeline_layout));

   VkPipelineShaderStageCreateInfo stage_create_info = {0};
   stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
   stage_create_info.module = compute_shader_module;
   stage_create_info.pName = "main";

   VkComputePipelineCreateInfo compute_pipeline_create_info = {0};
   compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   compute_pipeline_create_info.layout = compute_pipeline_layout;
   compute_pipeline_create_info.stage = stage_create_info;

   compute_effect gradient = {0};
   gradient.name = "gradient";
   gradient.layout = compute_pipeline_layout;
   gradient.constants.data[0] = (vec4){1, 0, 0, 1};
   gradient.constants.data[1] = (vec4){0, 0, 1, 1};

   VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, 0, &gradient.pipeline));

   vk.background_effect = gradient;

   // Initialize triangle pipeline.
   VkShaderModule vertex_shader_module;
   load_shader_module(&vertex_shader_module, vk.device, arena, "triangle.vert.spv");

   VkShaderModule fragment_shader_module;
   load_shader_module(&fragment_shader_module, vk.device, arena, "triangle.frag.spv");

   VkPipelineLayoutCreateInfo triangle_layout_info = {0};
   triangle_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   triangle_layout_info.pSetLayouts = &layout;
   triangle_layout_info.setLayoutCount = 1;

   VkPipelineLayout triangle_pipeline_layout;
   VK_CHECK(vkCreatePipelineLayout(vk.device, &triangle_layout_info, 0, &triangle_pipeline_layout));

   vulkan_pipeline_configuration triangle_pipeline_config = {0};
   initialize_pipeline_config(&triangle_pipeline_config);

   triangle_pipeline_config.layout = triangle_pipeline_layout;
   triangle_pipeline_config.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   triangle_pipeline_config.input_assembly.primitiveRestartEnable = VK_FALSE;

   triangle_pipeline_config.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
   triangle_pipeline_config.rasterizer.lineWidth = 1.0f;
   triangle_pipeline_config.rasterizer.cullMode = VK_CULL_MODE_NONE;
   triangle_pipeline_config.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

   triangle_pipeline_config.shader_stages[0] = (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertex_shader_module,
      .pName = "main",
   };
   triangle_pipeline_config.shader_stages[1] = (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragment_shader_module,
      .pName = "main",
   };

   triangle_pipeline_config.multisampling.sampleShadingEnable = VK_FALSE;
   triangle_pipeline_config.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
   triangle_pipeline_config.multisampling.minSampleShading = 1.0f;
   triangle_pipeline_config.multisampling.pSampleMask = 0;
   triangle_pipeline_config.multisampling.alphaToCoverageEnable = VK_FALSE;
   triangle_pipeline_config.multisampling.alphaToOneEnable = VK_FALSE;

   triangle_pipeline_config.color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
   triangle_pipeline_config.color_blend_attachment.blendEnable = VK_FALSE;

   triangle_pipeline_config.color_attachment_format = vk.draw_image.format;
   triangle_pipeline_config.rendering_info.colorAttachmentCount = 1;
   triangle_pipeline_config.rendering_info.pColorAttachmentFormats = &triangle_pipeline_config.color_attachment_format;

   triangle_pipeline_config.rendering_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
   triangle_pipeline_config.depth_stencil.depthTestEnable = VK_FALSE;
   triangle_pipeline_config.depth_stencil.depthWriteEnable = VK_FALSE;
   triangle_pipeline_config.depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
   triangle_pipeline_config.depth_stencil.depthBoundsTestEnable = VK_FALSE;
   triangle_pipeline_config.depth_stencil.stencilTestEnable = VK_FALSE;
   triangle_pipeline_config.depth_stencil.front = (VkStencilOpState){0};
   triangle_pipeline_config.depth_stencil.back = (VkStencilOpState){0};
   triangle_pipeline_config.depth_stencil.minDepthBounds = 0.f;
   triangle_pipeline_config.depth_stencil.maxDepthBounds = 1.f;

   vk.triangle_pipeline = create_pipeline(&triangle_pipeline_config, vk.device);

   // Initialize mesh pipeline.
   VkShaderModule vertex_mesh_shader_module;
   load_shader_module(&vertex_mesh_shader_module, vk.device, arena, "triangle_mesh.vert.spv");

   VkShaderModule fragment_mesh_shader_module;
   load_shader_module(&fragment_mesh_shader_module, vk.device, arena, "triangle_mesh.frag.spv");

   VkPipelineLayoutCreateInfo mesh_layout_info = {0};
   mesh_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   mesh_layout_info.pSetLayouts = &layout;
   mesh_layout_info.setLayoutCount = 1;

   VkPushConstantRange mesh_push_constant_range = {0};
   mesh_push_constant_range.offset = 0;
   mesh_push_constant_range.size = sizeof(mesh_push_constants);
   mesh_push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

   mesh_layout_info.pPushConstantRanges = &mesh_push_constant_range;
   mesh_layout_info.pushConstantRangeCount = 1;

   VK_CHECK(vkCreatePipelineLayout(vk.device, &mesh_layout_info, 0, &vk.mesh_pipeline_layout));

   vulkan_pipeline_configuration mesh_pipeline_config = {0};
   initialize_pipeline_config(&mesh_pipeline_config);

   mesh_pipeline_config.layout = vk.mesh_pipeline_layout;
   mesh_pipeline_config.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   mesh_pipeline_config.input_assembly.primitiveRestartEnable = VK_FALSE;

   mesh_pipeline_config.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
   mesh_pipeline_config.rasterizer.lineWidth = 1.0f;
   mesh_pipeline_config.rasterizer.cullMode = VK_CULL_MODE_NONE;
   mesh_pipeline_config.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

   mesh_pipeline_config.shader_stages[0] = (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertex_mesh_shader_module,
      .pName = "main",
   };
   mesh_pipeline_config.shader_stages[1] = (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragment_mesh_shader_module,
      .pName = "main",
   };

   mesh_pipeline_config.multisampling.sampleShadingEnable = VK_FALSE;
   mesh_pipeline_config.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
   mesh_pipeline_config.multisampling.minSampleShading = 1.0f;
   mesh_pipeline_config.multisampling.pSampleMask = 0;
   mesh_pipeline_config.multisampling.alphaToCoverageEnable = VK_FALSE;
   mesh_pipeline_config.multisampling.alphaToOneEnable = VK_FALSE;

   mesh_pipeline_config.color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
   mesh_pipeline_config.color_blend_attachment.blendEnable = VK_FALSE;

   mesh_pipeline_config.color_attachment_format = vk.draw_image.format;
   mesh_pipeline_config.rendering_info.colorAttachmentCount = 1;
   mesh_pipeline_config.rendering_info.pColorAttachmentFormats = &mesh_pipeline_config.color_attachment_format;

   mesh_pipeline_config.rendering_info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
   mesh_pipeline_config.depth_stencil.depthTestEnable = VK_FALSE;
   mesh_pipeline_config.depth_stencil.depthWriteEnable = VK_FALSE;
   mesh_pipeline_config.depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
   mesh_pipeline_config.depth_stencil.depthBoundsTestEnable = VK_FALSE;
   mesh_pipeline_config.depth_stencil.stencilTestEnable = VK_FALSE;
   mesh_pipeline_config.depth_stencil.front = (VkStencilOpState){0};
   mesh_pipeline_config.depth_stencil.back = (VkStencilOpState){0};
   mesh_pipeline_config.depth_stencil.minDepthBounds = 0.f;
   mesh_pipeline_config.depth_stencil.maxDepthBounds = 1.f;

   vk.mesh_pipeline = create_pipeline(&mesh_pipeline_config, vk.device);

   // Initialize IMGUI.
   VkCommandPool immediate_command_pool;
   VK_CHECK(vkCreateCommandPool(vk.device, &command_pool_info, 0, &immediate_command_pool));

   VkCommandBufferAllocateInfo command_allocate_info = {0};
   command_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command_allocate_info.commandPool = immediate_command_pool;
   command_allocate_info.commandBufferCount = 1;
   command_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

   VK_CHECK(vkAllocateCommandBuffers(vk.device, &command_allocate_info, &vk.immediate_command_buffer));
   VK_CHECK(vkCreateFence(vk.device, &fence_info, 0, &vk.immediate_fence));

   initialize_imgui(&vk);

   vertex vertices[4] = {0};
   vertices[0].position = (vec3){0.5, -0.5, 0};
   vertices[1].position = (vec3){0.5, 0.5, 0};
   vertices[2].position = (vec3){-0.5, -0.5, 0};
   vertices[3].position = (vec3){-0.5, 0.5, 0};

   vertices[0].color = (vec4){0, 0, 0, 1};
   vertices[1].color = (vec4){0.5, 0.5, 0.5, 1};
   vertices[2].color = (vec4){1, 0, 0, 1};
   vertices[3].color = (vec4){0, 1, 0, 1};

   u32 indices[6] = {0};
   indices[0] = 0;
   indices[1] = 1;
   indices[2] = 2;
   indices[3] = 2;
   indices[4] = 1;
   indices[5] = 3;

   vulkan_mesh mesh_buffers = push_mesh(&vk, vertices, countof(vertices), indices, countof(indices));

   // Render loop.
   while(!window_should_close(&vk))
   {
      vulkan_frame_commands *frame = vk.frame_commands + (vk.frame_count % countof(vk.frame_commands));

      vk.draw_extent.width = vk.draw_image.extent.width;
      vk.draw_extent.height = vk.draw_image.extent.height;

      VK_CHECK(vkWaitForFences(vk.device, 1, &frame->render_fence, 1, UINT64_MAX));
      VK_CHECK(vkResetFences(vk.device, 1, &frame->render_fence));

      u32 swapchain_image_index;
      VK_CHECK(vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, frame->swapchain_semaphore, 0, &swapchain_image_index));

      VkCommandBuffer cmd = frame->commands;
      VK_CHECK(vkResetCommandBuffer(cmd, 0));

      VkCommandBufferBeginInfo begin_info = {0};
      begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

      // Draw background.
      transition_image(cmd, vk.draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
      draw_background(&vk, &descriptor_set, cmd);

      transition_image(cmd, vk.draw_image.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      draw_geometry(&vk, cmd, mesh_buffers.vertex_address, mesh_buffers.indices.buffer);
      draw_imgui(&vk, cmd, vk.draw_image.view);

      transition_image(cmd, vk.draw_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      transition_image(cmd, vk.swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      copy_image(cmd, vk.draw_image.image, vk.swapchain_images[swapchain_image_index], vk.draw_extent, vk.swapchain_extent);

      transition_image(cmd, vk.swapchain_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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

   deinitialize_imgui(&vk);
   vkDestroyFence(vk.device, vk.immediate_fence, 0);
   vkDestroyCommandPool(vk.device, immediate_command_pool, 0);

   vmaDestroyBuffer(vk.allocator, mesh_buffers.indices.buffer, mesh_buffers.indices.allocation);
   vmaDestroyBuffer(vk.allocator, mesh_buffers.vertices.buffer, mesh_buffers.vertices.allocation);

   vkDestroyShaderModule(vk.device, compute_shader_module, 0);
   vkDestroyShaderModule(vk.device, vertex_shader_module, 0);
   vkDestroyShaderModule(vk.device, fragment_shader_module, 0);
   vkDestroyShaderModule(vk.device, vertex_mesh_shader_module, 0);
   vkDestroyShaderModule(vk.device, fragment_mesh_shader_module, 0);

   vkDestroyPipelineLayout(vk.device, vk.background_effect.layout, 0);
   vkDestroyPipelineLayout(vk.device, triangle_pipeline_layout, 0);
   vkDestroyPipelineLayout(vk.device, vk.mesh_pipeline_layout, 0);

   vkDestroyPipeline(vk.device, vk.background_effect.pipeline, 0);
   vkDestroyPipeline(vk.device, vk.triangle_pipeline, 0);
   vkDestroyPipeline(vk.device, vk.mesh_pipeline, 0);

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
