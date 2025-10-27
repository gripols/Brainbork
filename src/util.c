#include "util.h"

void OpcodeVector_init(OpcodeVector *vec)
{
	vec->data = NULL;
	vec->size = 0;
	vec->capacity = 0;
}

bool OpcodeVector_push_back(OpcodeVector *vec, opcode val)
{
	if (vec->size >= vec->capacity) {
		size_t new_capacity =
			vec->capacity == 0 ? 8 : vec->capacity * 2;
		opcode *new_data = (opcode *)realloc(
			vec->data, new_capacity * sizeof(opcode));
		if (!new_data) {
			perror("ERROR: Failed to reallocate opcode vector");
			return false;
		}
		vec->data = new_data;
		vec->capacity = new_capacity;
	}
	vec->data[vec->size++] = val;
	return true;
}

void OpcodeVector_free(OpcodeVector *vec)
{
	free(vec->data);
	vec->data = NULL;
	vec->size = 0;
	vec->capacity = 0;
}

void SizeTStack_init(SizeTStack *stk)
{
	stk->data = NULL;
	stk->size = 0;
	stk->capacity = 0;
}

int SizeTStack_empty(const SizeTStack *stk)
{
	return stk->size == 0;
}

bool SizeTStack_push(SizeTStack *stk, size_t val)
{
	if (stk->size >= stk->capacity) {
		size_t new_capacity =
			stk->capacity == 0 ? 8 : stk->capacity * 2;
		size_t *new_data = (size_t *)realloc(
			stk->data, new_capacity * sizeof(size_t));
		if (!new_data) {
			perror("Failed to reallocate size_t stack");
			return false;
		}
		stk->data = new_data;
		stk->capacity = new_capacity;
	}
	stk->data[stk->size++] = val;
	return true;
}

void SizeTStack_pop(SizeTStack *stk)
{
	if (stk->size > 0) {
		stk->size--;
	}
}

bool SizeTStack_top(const SizeTStack *stk, size_t *out_val)
{
	if (stk->size > 0) {
		*out_val = stk->data[stk->size - 1];
		return true;
	}
	fprintf(stderr, "Error: Accessing top of empty stack.\n");
	return false;
}

void SizeTStack_free(SizeTStack *stk)
{
	free(stk->data);
	stk->data = NULL;
	stk->size = 0;
	stk->capacity = 0;
}

void LambdaStack_init(LambdaStack *stk)
{
	stk->data = NULL;
	stk->size = 0;
	stk->capacity = 0;
}

bool LambdaStack_push(LambdaStack *stk, Lambda val)
{
	if (stk->size >= stk->capacity) {
		size_t new_capacity =
			stk->capacity == 0 ? 8 : stk->capacity * 2;
		Lambda *new_data = (Lambda *)realloc(
			stk->data, new_capacity * sizeof(Lambda));
		if (!new_data) {
			perror("Failed to reallocate lambda stack");
			return false;
		}
		stk->data = new_data;
		stk->capacity = new_capacity;
	}
	stk->data[stk->size++] = val;
	return true;
}

bool LambdaStack_top(const LambdaStack *stk, Lambda *out_val)
{
	if (stk->size > 0) {
		*out_val = stk->data[stk->size - 1];
		return true;
	}
	return false;
}

void LambdaStack_free(LambdaStack *stk)
{
	free(stk->data);
	stk->data = NULL;
	stk->size = 0;
	stk->capacity = 0;
}

void CallStack_init(CallStack *stk)
{
	stk->data = NULL;
	stk->size = 0;
	stk->capacity = 0;
}

bool CallStack_push(CallStack *stk, CallFrame val)
{
	if (stk->size >= stk->capacity) {
		size_t new_capacity =
			stk->capacity == 0 ? 8 : stk->capacity * 2;
		CallFrame *new_data = (CallFrame *)realloc(
			stk->data, new_capacity * sizeof(CallFrame));
		if (!new_data) {
			perror("Failed to reallocate call stack");
			return false;
		}
		stk->data = new_data;
		stk->capacity = new_capacity;
	}
	stk->data[stk->size++] = val;
	return true;
}

bool CallStack_pop(CallStack *stk, CallFrame *out_val)
{
	if (stk->size > 0) {
		stk->size--;
		*out_val = stk->data[stk->size];
		return true;
	}
	return false;
}

void CallStack_free(CallStack *stk)
{
	free(stk->data);
	stk->data = NULL;
	stk->size = 0;
	stk->capacity = 0;
}
