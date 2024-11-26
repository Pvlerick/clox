.PHONY: build run mmm test-mmm

objects = src/chunk.c src/debug.c src/line.c src/memory.c src/value.c src/vm.c src/stack.c src/mmm.c
mmm_linker_options = -Xlinker --wrap -Xlinker malloc -Xlinker --wrap -Xlinker free -Xlinker --wrap -Xlinker realloc

build:
	gcc -std=c2x $(mmm_linker_options) -o clox src/main.c $(objects)

clean:
	rm -f clox test-mmm

run:
	$(MAKE) clean
	$(MAKE) build
	./clox

debug:
	$(MAKE) clean
	gcc -std=c2x $(mmm_linker_options) -D DEBUG -D DEBUG_TRACE_EXECUTION -o clox src/main.c $(objects)
	./clox

trace:
	$(MAKE) clean
	gcc -std=c2x $(mmm_linker_options) -D DEBUG -D DEBUG_TRACE_MEMORY -D DEBUG_TRACE_EXECUTION -o clox src/main.c $(objects)
	./clox

test-mmm:
	$(MAKE) clean
	gcc -std=c2x $(mmm_linker_options) -D DEBUG -o test-mmm src/test_mmm.c $(objects)
	./test-mmm

run-nommm:
	$(MAKE) clean
	gcc -std=c2x -o clox src/main.c $(objects)
	./clox
