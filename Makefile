.PHONY: build run mmm test-mmm

objects = src/main.c src/chunk.c src/debug.c src/line.c src/memory.c src/value.c src/vm.c src/stack.c src/compiler.c src/scanner.c src/mmm.c 
mmm_linker_options = -Xlinker --wrap -Xlinker malloc -Xlinker --wrap -Xlinker free -Xlinker --wrap -Xlinker realloc

build:
	gcc -std=c2x $(mmm_linker_options) -o clox $(objects)

build-debug:
	gcc -std=c2x $(mmm_linker_options) -D DEBUG -D DEBUG_TRACE_EXECUTION -D DEBUG_PRINT_CODE -o clox $(objects)

build-trace:
	gcc -std=c2x $(mmm_linker_options) -D DEBUG -D DEBUG_TRACE_MEMORY -D DEBUG_TRACE_EXECUTION -o clox $(objects)

build-nommm:
	gcc -std=c2x -o clox $(objects)

clean:
	rm -f clox test-mmm

run:
	$(MAKE) clean
	$(MAKE) build
	./clox

debug:
	$(MAKE) clean
	$(MAKE) build-debug
	./clox

trace:
	$(MAKE) clean
	$(MAKE) build-trace
	./clox

test-mmm:
	$(MAKE) clean
	gcc -std=c2x $(mmm_linker_options) -D DEBUG -o test-mmm src/test_mmm.c $(objects)
	./test-mmm

run-nommm:
	$(MAKE) clean
	$(MAKE) build-nommm
	./clox
