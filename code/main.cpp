#define Array_Count(Array) (sizeof(Array) / sizeof((Array)[0]))

#include "vulkan_context.h"
#include "vulkan_context.cpp"

int main(void)
{
   vulkan_context VK = {};
   VK.Initialize("vkguide.dev", 800, 600);

   return(0);
}
