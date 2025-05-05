CFLAGS = -ggdb -Wall -std=c99 -I.
CXXFLAGS = -ggdb -Wall -I.
LDFLAGS = -L./build -lm -lstdc++ -lvulkan -limgui `pkg-config --libs sdl3`

# NOTE: I don't want to deal with build rules in make ever, so it's on you to
# run `make imgui` before the first build in order to compile the static
# library.

compile:
	mkdir -p build
	glslc -o shaders/gradient.comp.spv shaders/gradient.comp
	glslc -o shaders/gradient_color.comp.spv shaders/gradient_color.comp
	glslc -o shaders/triangle.vert.spv shaders/triangle.vert
	glslc -o shaders/triangle.frag.spv shaders/triangle.frag
	glslc -o shaders/triangle_mesh.vert.spv shaders/triangle_mesh.vert
	glslc -o shaders/triangle_mesh.frag.spv shaders/triangle_mesh.frag

	$(CC) -c -o build/vma.o -DVMA_IMPLEMENTATION -xc++ dependencies/vk_mem_alloc.h
	$(CC) -c -o build/wnd.o $(CXXFLAGS) src/window_creation.cpp `pkg-config --cflags sdl3`
	$(CC) -c -o build/main.o $(CFLAGS) src/main.c

	$(CC) -o build/vk build/main.o build/wnd.o build/vma.o $(LDFLAGS)

imgui:
	$(CC) -c -o build/imgui.o             $(CXXFLAGS) dependencies/imgui.cpp
	$(CC) -c -o build/imgui_impl_sdl3.o   $(CXXFLAGS) dependencies/imgui_impl_sdl3.cpp
	$(CC) -c -o build/imgui_impl_vulkan.o $(CXXFLAGS) dependencies/imgui_impl_vulkan.cpp
	$(CC) -c -o build/imgui_draw.o        $(CXXFLAGS) dependencies/imgui_draw.cpp
	$(CC) -c -o build/imgui_tables.o      $(CXXFLAGS) dependencies/imgui_tables.cpp
	$(CC) -c -o build/imgui_widgets.o     $(CXXFLAGS) dependencies/imgui_widgets.cpp
	$(AR) rcs build/libimgui.a build/imgui*.o

run:
	./build/vk
