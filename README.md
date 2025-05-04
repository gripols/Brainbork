# Buslang: A 102 Flavored Brainfuck Derivative

Brainfuck but 102. Functionally equivalent to Brainfuck, except the syntax is just 102.

## Language Overview

Buslang operates on a memory array with a data pointer, just like Brainfuck. Each command 
manipulates the memory pointer or data in some way. Here's the table between traditional 
Brainfuck operators and the Buslang equivalents:

| Brainfuck Operator | Buslang                  | Explanation                                                                 |
|--------------------|--------------------------|-----------------------------------------------------------------------------|
| `>`                | `ROUTE`                  | Move the memory pointer to the right                                       |
| `<`                | `102`                    | Move the memory pointer to the left                                        |
| `+`                | `MARKHAM`                | Increment the memory cell at the pointer                                   |
| `-`                | `ROAD`                   | Decrement the memory cell at the pointer                                   |
| `.`                | `SOUTHBOUND`             | Output the character at the memory cell                                    |
| `,`                | `TOWARDS`                | Accept one byte of input and store it in the current memory cell           |
| `[`                | `WARDEN`                 | If the memory cell at the pointer is zero, jump forward to the command after the matching `]` |
| `]`                | `STATION`                | If the memory cell at the pointer is nonzero, jump back to the command after the matching `[` |

If you would like to convert a Brainfuck file into Buslang, compile [bf-bus](bf-bus.c)
and input your `.bf` file.

## Running Buslang Code

Run the [Makefile](Makefile) and select either the JIT-compiler `-j` or the interpreter `-i`.
**JIT only works on machines using X86/AMD64 architectures. If you are running this on an ARM-based machine, please use the interpreter.**
I've included some code in [examples](examples) that you can run. 

## Why did you make this lmao
This is what going into CS does to a mf. I figured it would be a good exercise for 
anyone studying low-level programming. Yes my code is ass, it will get better (I can only hope.)

## Future Plans
- Clean up the code and codebase structure
- Expand JIT to support ARM
- Add multithreading support to train AI or something idk
- idk if you have any good ideas DM me