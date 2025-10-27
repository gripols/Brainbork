#ifndef BF_VM_H
#define BF_VM_H

#include "util.h"

/* The main VM memory tape. */
extern uint8_t g_bf_mem[0x20000];

/* The runtime stack for defined lambdas (closures). */
extern LambdaStack g_lambda_stack;

/* The runtime call stack for function calls. */
extern CallStack g_call_stack;

/* Executes an OpcodeVector using a simple interpreter. */
void interpreter(const OpcodeVector *code);

#endif // BF_VM_H
