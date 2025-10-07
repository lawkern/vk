#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#define VC(Call)                                \
   do {                                         \
      VkResult Result = (Call);                 \
      if(Result != VK_SUCCESS)                  \
      {                                         \
      }                                         \
   } while(0)

struct vulkan_context
{
   VkInstance Instance;
   VkPhysicalDevice Physical_Device;
   VkDevice Device;
   VkSurfaceKHR Surface;

   SDL_Window *Window;

   bool Initialized;
   void Initialize(const char *Name, int Width, int Height);

private:
   void Initialize_Vulkan(const char *Name);
   void Initalize_Swapchain(void);
   void Intialize_Commands(void);
   void Initialize_Synchronization(void);
};
