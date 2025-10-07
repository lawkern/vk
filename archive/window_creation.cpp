#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "dependencies/imgui.h"
#include "dependencies/imgui_impl_sdl3.h"
#include "dependencies/imgui_impl_vulkan.h"

#include "vk.h"

static VkDescriptorPool imgui_pool;

EXTERN_C bool create_window(vulkan_context *vk, char *title, int width, int height)
{
   bool result = false;

   vk->window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN);
   if(vk->window)
   {
      result = SDL_Vulkan_CreateSurface((SDL_Window *)vk->window, vk->instance, 0, &vk->surface);
   }

   if(!result)
   {
      SDL_Log("SDL Error: %s", SDL_GetError());
   }

   return(result);
}

EXTERN_C bool get_window_dimensions(vulkan_context *vk, int *width, int *height)
{
   bool result = SDL_GetWindowSizeInPixels((SDL_Window *)vk->window, width, height);
   if(!result)
   {
      SDL_Log("SDL Error: %s", SDL_GetError());
   }
   return(result);
}

EXTERN_C bool window_should_close(vulkan_context *vk)
{
   bool result = false;

   SDL_Event event;
   while(SDL_PollEvent(&event))
   {
      if(event.type == SDL_EVENT_QUIT) result = true;
      if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) result = true;

      ImGui_ImplSDL3_ProcessEvent(&event);
   }

   ImGui_ImplVulkan_NewFrame();
   ImGui_ImplSDL3_NewFrame();
   ImGui::NewFrame();

   if(ImGui::Begin("background"))
   {
      compute_effect *effect = &vk->background_effect;
      ImGui::Text("Effect: %s", effect->name);
      ImGui::InputFloat4("data[0]", (float *)(effect->constants.data + 0));
      ImGui::InputFloat4("data[1]", (float *)(effect->constants.data + 1));
      ImGui::InputFloat4("data[2]", (float *)(effect->constants.data + 2));
      ImGui::InputFloat4("data[3]", (float *)(effect->constants.data + 3));
   }
   ImGui::End();

   ImGui::Render();

   return(result);
}

EXTERN_C void initialize_imgui(vulkan_context *vk)
{
   VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
   };

   VkDescriptorPoolCreateInfo pool_info = {};
   pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
   pool_info.maxSets = 1000;
   pool_info.poolSizeCount = countof(pool_sizes);
   pool_info.pPoolSizes = pool_sizes;

   VK_CHECK(vkCreateDescriptorPool(vk->device, &pool_info, 0, &imgui_pool));

   ImGui::CreateContext();

   ImGui_ImplSDL3_InitForVulkan((SDL_Window *)vk->window);

   ImGui_ImplVulkan_InitInfo init_info = {};
   init_info.Instance = vk->instance;
   init_info.PhysicalDevice = vk->gpu;
   init_info.Device = vk->device;
   init_info.Queue = vk->graphics_queue;
   init_info.DescriptorPool = imgui_pool;
   init_info.MinImageCount = 3;
   init_info.ImageCount = 3;
   init_info.UseDynamicRendering = true;

   init_info.PipelineRenderingCreateInfo = {};
   init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
   init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
   init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &vk->draw_image.format;
   init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

   ImGui_ImplVulkan_Init(&init_info);
   ImGui_ImplVulkan_CreateFontsTexture();
}

EXTERN_C void draw_imgui(vulkan_context *vk, VkCommandBuffer cmd, VkImageView target)
{
   VkRenderingAttachmentInfo color_attachment = {};
   color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
   color_attachment.imageView = target;
   color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
   color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

   VkRenderingInfo rendering_info = {};
   rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
   rendering_info.flags = 0;
   rendering_info.renderArea = (VkRect2D){.extent=vk->swapchain_extent};
   rendering_info.layerCount = 1;
   rendering_info.viewMask = 0;
   rendering_info.colorAttachmentCount = 1;
   rendering_info.pColorAttachments = &color_attachment;
   rendering_info.pDepthAttachment = 0;
   rendering_info.pStencilAttachment = 0;

   vkCmdBeginRendering(cmd, &rendering_info);
   ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
   vkCmdEndRendering(cmd);
}

EXTERN_C void deinitialize_imgui(vulkan_context *vk)
{
   ImGui_ImplVulkan_Shutdown();
   vkDestroyDescriptorPool(vk->device, imgui_pool, 0);
}
