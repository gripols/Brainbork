#include "vm.h"

// Define the global VM state
uint8_t g_bf_mem[0x20000];
LambdaStack g_lambda_stack;
CallStack g_call_stack;

void interpreter(const OpcodeVector *code)
{
	clock_t begin = clock();
	memset(g_bf_mem, 0, sizeof(g_bf_mem));

	// Initialize runtime stacks
	LambdaStack_init(&g_lambda_stack);
	CallStack_init(&g_call_stack);

	uint32_t p = 0;
	size_t pc = 0;

	while (pc < code->size) {
		const opcode *op = &code->data[pc];
		switch (op->op) {
		case op_add:
			g_bf_mem[p] += op->num;
			pc++;
			break;
		case op_sub:
			g_bf_mem[p] -= op->num;
			pc++;
			break;
		case op_addp:
			p += op->num;
			pc++;
			break;
		case op_subp:
			p -= op->num;
			pc++;
			break;
		case op_jt:
			if (g_bf_mem[p])
				pc = op->num;
			else
				pc++;
			break;
		case op_jf:
			if (!g_bf_mem[p])
				pc = op->num;
			else
				pc++;
			break;
		case op_in:
			g_bf_mem[p] = getchar();
			pc++;
			break;
		case op_out:
			putchar(g_bf_mem[p]);
			pc++;
			break;
		case op_clear:
			g_bf_mem[p] = 0;
			pc++;
			break;

		case op_def_lambda: {
			Lambda lambda;
			lambda.start_pc =
				pc + 1; // Code starts after this opcode
			lambda.captured_p = p; // Capture current data pointer
			lambda.jit_addr = 0; // Not used by interpreter

			if (!LambdaStack_push(&g_lambda_stack, lambda)) {
				fprintf(stderr,
					"Interpreter runtime error: Lambda stack overflow\n");
				goto cleanup;
			}
			pc = op->num; // Jump past the function body
			break;
		}

		case op_ret: {
			CallFrame frame;
			if (!CallStack_pop(&g_call_stack, &frame)) {
				fprintf(stderr,
					"Interpreter runtime error: ')' without matching '!' call.\n");
				goto cleanup;
			}
			pc = frame.return_pc;
			p = frame.saved_p;
			break;
		}

		case op_call: {
			Lambda lambda;
			if (!LambdaStack_top(&g_lambda_stack, &lambda)) {
				fprintf(stderr,
					"Interpreter runtime error: '!' call with no defined lambda.\n");
				goto cleanup;
			}

			CallFrame frame;
			frame.return_pc =
				pc +
				1;
			frame.saved_p = p;

			if (!CallStack_push(&g_call_stack, frame)) {
				fprintf(stderr,
					"Interpreter runtime error: Call stack overflow\n");
				goto cleanup;
			}

			pc = lambda.start_pc;
			p = lambda.captured_p;
			break;
		}

		default:
			fprintf(stderr, "Unknown opcode: %d\n", op->op);
			pc++;
			break;
		}
	}

cleanup:
	LambdaStack_free(&g_lambda_stack);
	CallStack_free(&g_call_stack);

	clock_t end = clock();
	double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("\ninterpreter time usage: %fs\n", time_spent);
}
