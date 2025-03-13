.PHONY: build build-debug build-trace build-nommm clean run debug trace test-mmm test-table run-nommm

main = src/main.c
objects = src/chunk.c src/debug.c src/line.c src/memory.c src/value.c src/vm.c src/stack.c src/compiler.c src/scanner.c src/object.c src/table.c src/mmm.c
flags = -std=c2x
debug_flags = -D DEBUG -D DEBUG_TRACE_EXECUTION -D DEBUG_PRINT_CODE -D DEBUG_STRESS_GC 
trace_flags = -D DEBUG -D TRACE -D DEBUG_TRACE_MEMORY -D DEBUG_TRACE_EXECUTION -D DEBUG_PRINT_CODE -D DEBUG_STRESS_GC -D DEBUG_LOG_GC
mmm_linker_options = -Xlinker --wrap -Xlinker malloc -Xlinker --wrap -Xlinker free -Xlinker --wrap -Xlinker realloc

build:
	gcc $(flags) $(mmm_linker_options) -o clox $(main) $(objects) -lm

build-debug:
	gcc $(flags) $(mmm_linker_options) $(debug_flags) -o clox $(main) $(objects) -lm

build-trace:
	gcc $(flags) $(mmm_linker_options) $(trace_flags) -o clox $(main) $(objects) -lm

build-nommm:
	gcc $(flags) -o clox $(main) $(objects) -lm

clean:
	rm -f clox test-*

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
	gcc $(flags) $(mmm_linker_options) -D DEBUG -o test-mmm src/test_mmm.c $(objects)
	./test-mmm

test-table:
	gcc $(flags) -o test-table src/table_tests.c $(objects) -lm
	./test-table

test-all:
	$(MAKE) clean
	$(MAKE) build
	for file in test/*.lox; do \
		if [ -f "$$file" ]; then \
			echo -n "Running test $$file... "; \
			{ ./clox $$file > /dev/null && echo "ok"; } \
			|| { echo "failed"; exit 1; }; \
		fi; \
	done
	for file in test/*.xol; do \
		if [ -f "$$file" ]; then \
			echo -n "Running negative test $$file... "; \
			{ ./clox $$file > /dev/null 2>&1 || echo "ok"; } \
			|| { echo "failed"; exit 1; }; \
		fi; \
	done

run-nommm:
	$(MAKE) clean
	$(MAKE) build-nommm
	./clox $(FILE)
