# Brainfork: A High-Performance, Cross-Platform Brainf*ck Derivative

Brainbork is a high-performance interpreter and Just-In-Time (JIT) 
compiler for a Brainf*ck-compatible derivative. Written in C, it features a clean, 
modular architecture that generates machine code on-the-fly for x86-64 and aarch64 
(ARM64) systems. The entire VM state is encapsulated in a BfVm struct, eliminating 
global state and ensuring testability.

## Key Features
- Dual Execution Modes: Run code via a fast, portable interpreter `(-i)` or the high-performance JIT compiler `(-j)`.
- Cross-Platform JIT: Automatically detects x86-64 or aarch64 (ARM64) hosts and generates optimized native code.
- Peephole Optimization: A pre-compilation pass collapses common patterns like `[-]`and `[+]` into a single, efficient op_clear.
- Extended Syntax: Lambda Closures: Implements first-class, nestable functions (()) with true closure support (capturing the data pointers).

## Getting Started
The project requires only `gcc` (or `clang`) and `make`.

### Build
Clone the repository and run make:
```
git clone https://gripols/Brainfork.git
cd brainfork
make
```
This detects your host architecture (x86_64 or aarch64), builds the appropriate JIT backend, 
and creates the brainfork executable.

### Run
Execute any Brainf*ck or Brainfork file.

Usage:
```
./brainfork [options] <filename.bf>
Options:
-i: Interpreter Mode.
-j: JIT Mode (compiles to native assembly).
```
Example (examples/mandelbrot.bf):

#### Run with the JIT (Fastest)
```
./brainfork -j examples/mandelbrot.bf
```

#### Run with the Interpreter
```
./brainfork -i examples/mandelbrot.bf
```

### Extended Syntax: 
Brainfork adds three operators for stack-based functions:
- `(`: Define Lambda. Begins a function definition, capturing the current data pointer (p) as a closure.
- `)`: End Lambda. Marks the end of the function body.
- `!`: Call Lambda. Calls the most recently defined lambda. This peeks at the lambda stack (allowing multiple calls), 
pushes the current state to the call stack, and jumps to the lambda's code with its captured pointer.

## Licensing
Licensed under MIT license.

## Contributions
All contributions are welcome.
