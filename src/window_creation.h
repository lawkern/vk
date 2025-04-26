#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

typedef SDL_Window window;

window *create_window(char *title, int width, int height)
#ifdef WND_IMPLEMENTATION
{
   window *result = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN);
   return(result);
}
#else
;
#endif

_Bool create_window_surface(window *wnd, VkInstance instance, VkSurfaceKHR *surface)
#ifdef WND_IMPLEMENTATION
{
   _Bool result = SDL_Vulkan_CreateSurface(wnd, instance, 0, surface);
   if(!result)
   {
      SDL_Log("SDL Error: %s", SDL_GetError());
   }
   return(result);
}
#else
;
#endif

_Bool get_window_dimensions(window *wnd, int *width, int *height)
#ifdef WND_IMPLEMENTATION
{
   _Bool result = SDL_GetWindowSizeInPixels(wnd, width, height);
   if(!result)
   {
      SDL_Log("SDL Error: %s", SDL_GetError());
   }
   return(result);
}
#else
;
#endif

_Bool window_should_close(void)
#ifdef WND_IMPLEMENTATION
{
   _Bool result = 0;
   SDL_Event event;
   while(SDL_PollEvent(&event))
   {
      if(event.type == SDL_EVENT_QUIT) result = 1;
      if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) result = 1;
   }
   return(result);
}
#else
;
#endif
