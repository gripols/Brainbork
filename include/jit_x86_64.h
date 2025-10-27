#ifndef JIT_X86_64_H
#define JIT_X86_64_H

#include "util.h"

/**
 * @brief Compiles and executes an OpcodeVector using the x86-64 JIT.
 */
bool jit_exec_x86_64(const OpcodeVector *code);

#endif // JIT_X86_64_H
