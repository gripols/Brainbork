#ifndef JIT_RUNTIME_H
#define JIT_RUNTIME_H

#include "util.h"

/**
 * @brief Called by JIT for op_def_lambda. Pushes a new lambda onto the stack.
 * @param jit_addr The executable memory address of the compiled lambda.
 * @param captured_p The data pointer (p) at the time of definition.
 */
void jit_runtime_push_lambda(uint64_t jit_addr, uint32_t captured_p);

/**
 * @brief Called by JIT for op_call (before the call).
 * - Pushes the current state (return PC, data pointer) onto the call stack.
 * - Pops and returns the target Lambda (closure) from the lambda stack.
 *
 * @param current_p The current data pointer (e.g., rbx/x19).
 * @param return_pc The JIT'd return address (PC after the 'call' instruction).
 * @return The Lambda to be called.
 */
Lambda jit_runtime_pre_call(uint32_t current_p, uint64_t return_pc);

/**
 * @brief Called by JIT for op_ret (or after a lambda call returns).
 * - Pops and returns the saved CallFrame.
 *
 * @param[out] out_return_pc The JIT'd return address from the call frame.
 * @return The caller's saved data pointer (p).
 */
uint32_t jit_runtime_post_ret(uint64_t *out_return_pc);

#endif // JIT_RUNTIME_H
