#include "jit_runtime.h"
#include "vm.h"

void jit_runtime_push_lambda(uint64_t jit_addr, uint32_t captured_p)
{
	Lambda lambda;
	lambda.start_pc = 0; // Not used by JIT
	lambda.captured_p = captured_p;
	lambda.jit_addr = jit_addr;

	if (!LambdaStack_push(&g_lambda_stack, lambda)) {
		fprintf(stderr, "JIT runtime error: Lambda stack overflow\n");
		// this should trigger a safe exit
	}
}

Lambda jit_runtime_pre_call(uint32_t current_p, uint64_t return_pc)
{
	Lambda lambda;
	if (!LambdaStack_top(&g_lambda_stack, &lambda)) {
		fprintf(stderr,
			"JIT runtime error: '!' call with no defined lambda.\n");
		lambda.jit_addr = 0; // ret null lambda
		lambda.captured_p = 0;
		return lambda;
	}

	CallFrame frame;
	frame.return_pc = return_pc; // jit mem addr
	frame.saved_p = current_p;

	if (!CallStack_push(&g_call_stack, frame)) {
		fprintf(stderr, "JIT runtime error: Call stack overflow\n");
		lambda.jit_addr = 0; // ret null lambda
	}

	return lambda;
}

uint32_t jit_runtime_post_ret(uint64_t *out_return_pc)
{
	CallFrame frame;
	if (!CallStack_pop(&g_call_stack, &frame)) {
		fprintf(stderr,
			"JIT runtime error: Return from non-existent call.\n");
		*out_return_pc = 0;
		return 0;
	}

	*out_return_pc = frame.return_pc; // JIT'd ret addr
	return frame.saved_p; // caller saved data ptr
}
