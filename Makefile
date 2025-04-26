CFLAGS = -ggdb -Wall -std=c99
LDFLAGS = -lm -lstdc++ -lvulkan `pkg-config --libs sdl3`

compile:
	mkdir -p build
	$(CC) -c -o build/vma.o -DVMA_IMPLEMENTATION -xc++ -ggdb src/vk_mem_alloc.h
	$(CC) -c -o build/wnd.o -DWND_IMPLEMENTATION -xc $(CFLAGS) src/window_creation.h `pkg-config --cflags sdl3`
	$(CC) -c -o build/main.o $(CFLAGS) src/main.c
	$(CC) -o build/vk build/main.o build/wnd.o build/vma.o $(LDFLAGS)

run:
	./build/vk
