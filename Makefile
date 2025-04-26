compile:
	gcc -o wnd.o -c -g -xc   -DWND_IMPLEMENTATION src/window_creation.h `pkg-config --cflags sdl3`
	g++ -o vma.o -c -g -xc++ -DVMA_IMPLEMENTATION src/vk_mem_alloc.h
	gcc -o main.o -c -g src/main.c
	gcc -o vk main.o wnd.o vma.o -lm -lstdc++ -lvulkan `pkg-config --libs sdl3`
	rm main.o vma.o

run:
	./vk
