CXXFLAGS = -g3 -Wall -Wextra
SDL = $$(pkg-config --cflags --libs sdl3)

compile:
	mkdir -p build
	eval $(CXX) -o build/vkguide_dev code/main.cpp $(CXXFLAGS) $(SDL) -lvulkan

debug:
	cd build && gdb vkguide_dev
