#ifndef BF_UTIL_H
#define BF_UTIL_H

#include "brainfork.h"

typedef struct OpcodeVector {
	opcode *data;
	size_t size;
	size_t capacity;
} OpcodeVector;

void OpcodeVector_init(OpcodeVector *vec);
bool OpcodeVector_push_back(OpcodeVector *vec, opcode val);
void OpcodeVector_free(OpcodeVector *vec);

typedef struct SizeTStack {
	size_t *data;
	size_t size;
	size_t capacity;
} SizeTStack;

void SizeTStack_init(SizeTStack *stk);
int SizeTStack_empty(const SizeTStack *stk);
bool SizeTStack_push(SizeTStack *stk, size_t val);
void SizeTStack_pop(SizeTStack *stk);
bool SizeTStack_top(const SizeTStack *stk, size_t *out_val);
void SizeTStack_free(SizeTStack *stk);

/*
* @brief Represents a lambda function in the Brainfork VM.
* size_t start_pc: opcode index where the lambda's code starts.
* uint32_t captured_p: data pointer (p) at the moment of definition.
* uint64_t jit_addr: actual memory address of the compiled function.
*/
typedef struct {
	size_t start_pc;
	uint32_t captured_p;
	uint64_t jit_addr;
} Lambda;

/*
* @brief Represents a call frame for managing function calls.
* size_t return_pc: the opcode index to return to after the call from the lambda.
* uint32_t saved_p: the data pointer (p) to restore upon return.
*/
typedef struct {
	size_t return_pc;
	uint32_t saved_p;
} CallFrame;

/*
* @brief Stack structure for managing Lambda instances.
* size_t size: current number of elements in the stack.
* size_t capacity: total allocated capacity of the stack.
*/
typedef struct {
	Lambda *data;
	size_t size;
	size_t capacity;
} LambdaStack;

void LambdaStack_init(LambdaStack *stk);
bool LambdaStack_push(LambdaStack *stk, Lambda val);
bool LambdaStack_top(const LambdaStack *stk, Lambda *out_val);
void LambdaStack_free(LambdaStack *stk);

/*
* @brief Stack structure for managing CallFrame instances.
* size_t size: current number of elements in the stack.
* size_t capacity: total allocated capacity of the stack.
*/
typedef struct {
	CallFrame *data;
	size_t size;
	size_t capacity;
} CallStack;

void CallStack_init(CallStack *stk);
bool CallStack_push(CallStack *stk, CallFrame val);
bool CallStack_pop(CallStack *stk, CallFrame *out_val);
void CallStack_free(CallStack *stk);

#endif // BF_UTIL_H
