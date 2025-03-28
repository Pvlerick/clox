# CLOX

Second part of the https://craftinginterpreters.com/ bool - a bytecode VM in C for the Lox language. 

This implementation contains all the challenges solved in the mainline, so it differes significantly from the original one at https://github.com/munificent/craftinginterpreters/tree/master/c

Only the challenges that changes the language semantics (such as BETA's ```inner()``` were left in their own branch). I also skipped the different GC implementation, maybe one day I'll go back to it!

This book is an amazing read, go buy it!

## Running

To run a specific file:
```
make run FILE=sample.lox
```

To run with DEBUG flags and see much more output:
```
make debug FILE=sample.lox
```

To run with TRAC flags and see tons of output, including memory allocation (hardcore mode!):
```
make trace FILE=sample.lox
```

## Testing

To run the "official" test suite:

```
make test-suite
```

## Profiling

```
sudo perf record ./clox test/benchmark_hashtable.lox

perf report
```
