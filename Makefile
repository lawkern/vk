compile:
	$(CC) -o vk -ggdb ./src/main.c -lm `pkg-config --cflags --libs vulkan` `pkg-config --cflags --libs sdl3`

run:
	./vk
