compile:
	$(CC) -o vk -ggdb ./src/main.c `pkg-config --cflags --libs vulkan` `pkg-config --cflags --libs sdl3`

run:
	./vk
