# CLOX

Second part of the https://craftinginterpreters.com/ bool - a bytecode VM in C for the Lox language.

This book is amazing, go buy it!

## Profiling

```
sudo perf record ./clox test/benchmark_hashtable.lox

perf report
```
