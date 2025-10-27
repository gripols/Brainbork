#ifndef BF_COMPILER_H
#define BF_COMPILER_H

#include "util.h"

/**
 * @brief Scans a Brainfork source string into an OpcodeVector.
 */
bool scanner(const char *s, OpcodeVector *out_code);

/**
 * @brief Performs peephole optimizations on an OpcodeVector.
 */
bool optimize(const OpcodeVector *in_code, OpcodeVector *out_code);

#endif // BF_COMPILER_H
