#ifndef JIT_AARCH64_H
#define JIT_AARCH64_H

#include "util.h"

/*
 * @brief Just compiles and executes the OpcodeVector using the ARM JIT (which
 * in of itself is just a translation of the X86 JIT).
 */
bool jit_exec_aarch64(const OpcodeVector *code);

#endif // JIT_AARCH64_H
