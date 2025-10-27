#ifndef BRAINFORK_H
#define BRAINFORK_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>

typedef enum {
	op_add, // +
	op_sub, // -
	op_addp, // >
	op_subp, // <
	op_jt, // ] (Jump if true / non-zero)
	op_jf, // [ (Jump if false / zero)
	op_in, // ,
	op_out, // .
	op_clear, // [-]
	op_def_lambda, // ( - Define a lambda
	op_ret, // ) - Return from lambda
	op_call // ! - Call last-defined lambda
} optype_t;

typedef struct {
	optype_t op;
	uint32_t num;
} opcode;

// full forward declarations for key data structures are in util.h)
struct OpcodeVector;
struct SizeTStack;

#endif // BRAINFORK_H
