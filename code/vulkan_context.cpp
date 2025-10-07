void vulkan_context::Initialize(const char *Name, int Width, int Height)
{
   SDL_Init(SDL_INIT_VIDEO);
   Window = SDL_CreateWindow(Name, Width, Height, SDL_WINDOW_VULKAN);

   Initialize_Vulkan(Name);
   Initalize_Swapchain();
   Intialize_Commands();
   Initialize_Synchronization();

   Initialized = true;
}

void vulkan_context::Initialize_Vulkan(const char *Name)
{
   VkApplicationInfo Application_Info = {};
   Application_Info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   Application_Info.pApplicationName = Name;
   Application_Info.applicationVersion = 1;
   Application_Info.pEngineName = "None";
   Application_Info.engineVersion = 0;
   Application_Info.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);


   const char *Instance_Extension_Names[] =
   {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
   };
   const char *Instance_Layer_Names[] =
   {
      "VK_LAYER_KHRONOS_validation"
   };

   VkInstanceCreateInfo Create_Info = {};
   Create_Info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   Create_Info.pApplicationInfo = &Application_Info;
   Create_Info.enabledLayerCount = Array_Count(Instance_Layer_Names);
   Create_Info.ppEnabledLayerNames = Instance_Layer_Names;
   Create_Info.enabledExtensionCount = Array_Count(Instance_Extension_Names);
   Create_Info.ppEnabledExtensionNames = Instance_Extension_Names;

   VC(vkCreateInstance(&Create_Info, 0, &Instance));
}

void vulkan_context::Initalize_Swapchain(void)
{
}

void vulkan_context::Intialize_Commands(void)
{
}

void vulkan_context::Initialize_Synchronization(void)
{
}
