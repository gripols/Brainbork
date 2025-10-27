#ifndef BF_JIT_H
#define BF_JIT_H

#include "util.h"

/**
 * @brief Executes an OpcodeVector using the JIT compiler.
 * * This function dispatches to the correct architecture-specific backend.
 */
bool jit(const OpcodeVector *code);

#endif // BF_JIT_H
