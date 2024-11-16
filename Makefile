.PHONY: build run mmm test-mmm

objects = src/chunk.c src/debug.c src/line.c src/memory.c src/value.c src/vm.c src/stack.c

build:
	gcc -std=c2x -o clox src/main.c $(objects)

run:
	$(MAKE) build
	./clox

debug:
	gcc -std=c2x -D DEBUG -D DEBUG_TRACE_EXECUTION -o clox src/main.c $(objects)
	./clox

trace:
	gcc -std=c2x -D DEBUG -D DEBUG_TRACE_MEMORY -D DEBUG_TRACE_EXECUTION -o clox src/main.c $(objects)
	./clox

test-mmm:
	gcc -std=c2x -D DEBUG -o test-mmm src/test_mmm.c $(objects)
	./test-mmm

run-nommm:
	gcc -std=c2x -D NO_MANUAL_MEMORY_MANAGEMENT -D DEBUG -o clox src/main.c $(objects)
	./clox
