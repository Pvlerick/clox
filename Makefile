.PHONY: build build-debug build-trace build-nommm clean run debug trace test-mmm run-nommm

objects = src/main.c src/chunk.c src/debug.c src/line.c src/memory.c src/value.c src/vm.c src/stack.c src/compiler.c src/scanner.c src/object.c src/table.c src/mmm.c
debug_flags = -D DEBUG -D DEBUG_TRACE_EXECUTION -D DEBUG_PRINT_CODE -D DEBUG_STRESS_GC -D DEBUG_LOG_GC
trace_flags = -D DEBUG -D TRACE -D DEBUG_TRACE_MEMORY -D DEBUG_TRACE_EXECUTION -D DEBUG_PRINT_CODE -D DEBUG_STRESS_GC -D DEBUG_LOG_GC
mmm_linker_options = -Xlinker --wrap -Xlinker malloc -Xlinker --wrap -Xlinker free -Xlinker --wrap -Xlinker realloc

build:
	gcc -std=c2x $(mmm_linker_options) -o clox $(objects) -lm

build-debug:
	gcc -std=c2x $(mmm_linker_options) $(debug_flags) -o clox $(objects) -lm

build-trace:
	gcc -std=c2x $(mmm_linker_options) $(trace_flags) -o clox $(objects) -lm

build-nommm:
	gcc -std=c2x -o clox $(objects) -lm

clean:
	rm -f clox test-mmm

run:
	$(MAKE) clean
	$(MAKE) build
	./clox $(FILE)

debug:
	$(MAKE) clean
	$(MAKE) build-debug
	./clox $(FILE)

trace:
	$(MAKE) clean
	$(MAKE) build-trace
	./clox $(FILE)

test-mmm:
	$(MAKE) clean
	gcc -std=c2x $(mmm_linker_options) -D DEBUG -o test-mmm src/test_mmm.c $(objects)
	./test-mmm

run-nommm:
	$(MAKE) clean
	$(MAKE) build-nommm
	./clox $(FILE)
