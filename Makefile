CFLAGS = -ggdb -Wall -std=c99 -I.
CXXFLAGS = -ggdb -Wall -I.
LDFLAGS = -L./build -lm -lstdc++ -lvulkan -limgui -lvma `pkg-config --libs sdl3`

# NOTE: I don't want to deal with build rules in make ever, so it's on you to
# run `make external` before the first build in order to compile the external
# libraries.

compile:
	mkdir -p build
	glslc -o build/gradient.comp.spv          src/shaders/gradient.comp
	glslc -o build/gradient_color.comp.spv    src/shaders/gradient_color.comp
	glslc -o build/triangle.vert.spv          src/shaders/triangle.vert
	glslc -o build/triangle.frag.spv          src/shaders/triangle.frag
	glslc -o build/triangle_mesh.vert.spv     src/shaders/triangle_mesh.vert
	glslc -o build/triangle_mesh.frag.spv     src/shaders/triangle_mesh.frag

	$(CC) -c -o build/wnd.o $(CXXFLAGS) src/window_creation.cpp `pkg-config --cflags sdl3`
	$(CC) -c -o build/main.o $(CFLAGS) src/main.c
	$(CC) -o build/vk build/main.o build/wnd.o $(LDFLAGS)

external:
	$(CC) -c -o build/imgui.o             $(CXXFLAGS) src/dependencies/imgui.cpp
	$(CC) -c -o build/imgui_impl_sdl3.o   $(CXXFLAGS) src/dependencies/imgui_impl_sdl3.cpp
	$(CC) -c -o build/imgui_impl_vulkan.o $(CXXFLAGS) src/dependencies/imgui_impl_vulkan.cpp
	$(CC) -c -o build/imgui_draw.o        $(CXXFLAGS) src/dependencies/imgui_draw.cpp
	$(CC) -c -o build/imgui_tables.o      $(CXXFLAGS) src/dependencies/imgui_tables.cpp
	$(CC) -c -o build/imgui_widgets.o     $(CXXFLAGS) src/dependencies/imgui_widgets.cpp
	$(AR) rcs build/libimgui.a build/imgui*.o

	$(CC) -c -o build/vma.o -DVMA_IMPLEMENTATION -ggdb -xc++ src/dependencies/vk_mem_alloc.h
	$(AR) rcs build/libvma.a build/vma.o

run:
	cd build; ./vk
