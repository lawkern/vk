CFLAGS = -ggdb -Wall -std=c99 -I.
CXXFLAGS = -ggdb -Wall -I.
LDFLAGS = -lm -lstdc++ -lvulkan `pkg-config --libs sdl3`

compile:
	mkdir -p build
	glslc -o shaders/gradient.comp.spv shaders/gradient.comp

	$(CC) -c -o build/imgui.o             $(CXXFLAGS) dependencies/imgui.cpp
	$(CC) -c -o build/imgui_impl_sdl3.o   $(CXXFLAGS) dependencies/imgui_impl_sdl3.cpp
	$(CC) -c -o build/imgui_impl_vulkan.o $(CXXFLAGS) dependencies/imgui_impl_vulkan.cpp
	$(CC) -c -o build/imgui_draw.o        $(CXXFLAGS) dependencies/imgui_draw.cpp
	$(CC) -c -o build/imgui_tables.o      $(CXXFLAGS) dependencies/imgui_tables.cpp
	$(CC) -c -o build/imgui_widgets.o     $(CXXFLAGS) dependencies/imgui_widgets.cpp
	$(CC) -c -o build/imgui_demo.o        $(CXXFLAGS) dependencies/imgui_demo.cpp

	$(CC) -c -o build/vma.o -DVMA_IMPLEMENTATION -xc++ dependencies/vk_mem_alloc.h
	$(CC) -c -o build/wnd.o $(CXXFLAGS) src/window_creation.cpp `pkg-config --cflags sdl3`
	$(CC) -c -o build/main.o $(CFLAGS) src/main.c

	$(CC) -o build/vk build/main.o build/wnd.o build/imgui*.o build/vma.o $(LDFLAGS)


run:
	./build/vk
