.PHONY: build run mmm test-mmm

objects = src/chunk.c src/debug.c src/line.c src/memory.c src/value.c

build:
	gcc -D MANUAL_MEMORY_MANAGEMENT -o clox src/main.c $(objects)

run:
	$(MAKE) build
	./clox

mmm:
	gcc -D MANUAL_MEMORY_MANAGEMENT -o clox src/main.c $(objects)
	./clox

test-mmm:
	gcc -D MANUAL_MEMORY_MANAGEMENT -o test-mmm src/test_mmm.c $(objects)
	./test-mmm
