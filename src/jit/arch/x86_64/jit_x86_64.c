#include "jit_x86_64.h"
#include "jit_common.h"
#include "jit_runtime.h"
#include "vm.h"

typedef enum {
	REG_RAX = 0,
	REG_RCX = 1,
	REG_RDX = 2,
	REG_RBX = 3,
	REG_RSP = 4,
	REG_RBP = 5,
	REG_RSI = 6,
	REG_RDI = 7,
	REG_R8 = 8,
	REG_R9 = 9,
	REG_R10 = 10,
	REG_R11 = 11,
	REG_R12 = 12,
	REG_R13 = 13,
	REG_R14 = 14,
	REG_R15 = 15,

	REG_AL = 0,
	REG_CL = 1,
	REG_DL = 2,
	REG_BL = 3,
} X86Reg;

#define REX_W_PREFIX 0x48
#define REX_R_PREFIX 0x44
#define REX_B_PREFIX 0x41

/**
 * @brief Pushes a REX prefix byte if necessary.
 * W: 64-bit operand
 * R: Extends ModRM 'reg' field
 * X: Extends SIB 'index' field (not used)
 * B: Extends ModRM 'r/m' field or SIB 'base' or Opcode 'reg'
 */
static bool jit_rex_prefix(JitBuffer *jit, bool W, bool R, bool X, bool B)
{
	uint8_t rex = 0x40;
	if (W)
		rex |= 0x08;
	if (R)
		rex |= 0x04;
	if (X)
		rex |= 0x02;
	if (B)
		rex |= 0x01;
	if (rex > 0x40) {
		return JitBuffer_push8(jit, rex);
	}
	return true;
}

static bool jit_mov_reg_imm64(JitBuffer *jit, X86Reg reg, uint64_t imm)
{
	bool B = (reg >= REG_R8);
	// REX.W + REX.B if needed
	if (!jit_rex_prefix(jit, true, false, false, B))
		return false;
	if (!JitBuffer_push8(jit, 0xb8 + (reg & 0x07)))
		return false;
	return JitBuffer_push64(jit, imm);
}
static bool jit_add_mem8_imm8(JitBuffer *jit, X86Reg reg, uint8_t imm)
{
	// This helper is only ever called with RBX, which doesn't need REX.B
	// If it were, we'd need to add REX.B prefix here.
	if (!JitBuffer_push8(jit, 0x80))
		return false;
	if (!JitBuffer_push8(jit, 0x00 + (reg & 0x07)))
		return false;
	return JitBuffer_push8(jit, imm);
}
static bool jit_sub_mem8_imm8(JitBuffer *jit, X86Reg reg, uint8_t imm)
{
	if (!JitBuffer_push8(jit, 0x80))
		return false;
	if (!JitBuffer_push8(jit, 0x28 + (reg & 0x07)))
		return false;
	return JitBuffer_push8(jit, imm);
}
static bool jit_add_reg_imm32(JitBuffer *jit, X86Reg reg, uint32_t imm)
{
	// This helper is only ever called with RBX.
	if (!jit_rex_prefix(jit, true, false, false, false))
		return false; // REX.W
	if (!JitBuffer_push8(jit, 0x81))
		return false;
	if (!JitBuffer_push8(jit, 0xc0 + (reg & 0x07)))
		return false;
	return JitBuffer_push32(jit, imm);
}
static bool jit_sub_reg_imm32(JitBuffer *jit, X86Reg reg, uint32_t imm)
{
	// This helper is only ever called with RBX.
	if (!jit_rex_prefix(jit, true, false, false, false))
		return false; // REX.W
	if (!JitBuffer_push8(jit, 0x81))
		return false;
	if (!JitBuffer_push8(jit, 0xe8 + (reg & 0x07)))
		return false;
	return JitBuffer_push32(jit, imm);
}
static bool jit_mov_mem8_reg8(JitBuffer *jit, X86Reg mem_reg, X86Reg val_reg)
{
	// Only called with RBX and AL. No REX prefix needed.
	if (!JitBuffer_push8(jit, 0x88))
		return false;
	return JitBuffer_push8(jit, 0x00 | ((val_reg & 0x07) << 3) |
					    (mem_reg & 0x07));
}
static bool jit_mov_reg8_mem8(JitBuffer *jit, X86Reg val_reg, X86Reg mem_reg)
{
	// Only called with AL and RBX. No REX prefix needed.
	if (!JitBuffer_push8(jit, 0x8a))
		return false;
	return JitBuffer_push8(jit, 0x00 | ((val_reg & 0x07) << 3) |
					    (mem_reg & 0x07));
}
static bool jit_test_reg8_reg8(JitBuffer *jit, X86Reg reg1, X86Reg reg2)
{
	// Only called with AL. No REX prefix needed.
	if (!JitBuffer_push8(jit, 0x84))
		return false;
	return JitBuffer_push8(jit,
			       0xc0 | ((reg2 & 0x07) << 3) | (reg1 & 0x07));
}
static bool jit_call_reg(JitBuffer *jit, X86Reg reg)
{
	bool B = (reg >= REG_R8);
	// REX.B (if needed)
	if (!jit_rex_prefix(jit, false, false, false, B))
		return false;
	if (!JitBuffer_push8(jit, 0xff))
		return false;
	return JitBuffer_push8(jit, 0xd0 + (reg & 0x07));
}
static bool jit_movsx_reg_mem8(JitBuffer *jit, X86Reg dest_reg, X86Reg mem_reg)
{
	// Only called with RDI/RCX (dest) and RBX (mem). No REX prefix needed.
	if (!JitBuffer_push8(jit, 0x0f))
		return false;
	if (!JitBuffer_push8(jit, 0xbe))
		return false;
	return JitBuffer_push8(jit, 0x00 | ((dest_reg & 0x07) << 3) |
					    (mem_reg & 0x07));
}
static bool jit_push_reg(JitBuffer *jit, X86Reg reg)
{
	bool B = (reg >= REG_R8);
	// REX.B (if needed)
	if (!jit_rex_prefix(jit, false, false, false, B))
		return false;
	return JitBuffer_push8(jit, 0x50 + (reg & 0x07));
}
static bool jit_pop_reg(JitBuffer *jit, X86Reg reg)
{
	bool B = (reg >= REG_R8);
	// REX.B (if needed)
	if (!jit_rex_prefix(jit, false, false, false, B))
		return false;
	return JitBuffer_push8(jit, 0x58 + (reg & 0x07));
}
static bool jit_mov_reg_reg(JitBuffer *jit, X86Reg dest, X86Reg src)
{
	bool R = (src >= REG_R8);
	bool B = (dest >= REG_R8);
	// REX.W + REX.R + REX.B
	if (!jit_rex_prefix(jit, true, R, false, B))
		return false;
	if (!JitBuffer_push8(jit, 0x89))
		return false;
	return JitBuffer_push8(jit, 0xc0 | ((src & 0x07) << 3) | (dest & 0x07));
}
static bool jit_ret(JitBuffer *jit)
{
	return JitBuffer_push8(jit, 0xc3);
}
static bool jit_mov_mem8_imm8(JitBuffer *jit, X86Reg reg, uint8_t imm)
{
	// Only called with RBX. No REX prefix needed.
	if (!JitBuffer_push8(jit, 0xc6))
		return false;
	if (!JitBuffer_push8(jit, 0x00 + (reg & 0x07)))
		return false;
	return JitBuffer_push8(jit, imm);
}
static bool jit_je(JitBuffer *jit, size_t target_opcode_index)
{
	uint8_t je_op[] = { 0x0f, 0x84, 0x00, 0x00, 0x00, 0x00 };
	if (!JitBuffer_add_jump_patch(jit, target_opcode_index, 0))
		return false;
	return JitBuffer_push_bytes(jit, je_op, sizeof(je_op));
}
static bool jit_jne(JitBuffer *jit, size_t target_opcode_index)
{
	uint8_t jne_op[] = { 0x0f, 0x85, 0x00, 0x00, 0x00, 0x00 };
	if (!JitBuffer_add_jump_patch(jit, target_opcode_index, 1))
		return false;
	return JitBuffer_push_bytes(jit, jne_op, sizeof(jne_op));
}

/**
 * @brief x86-64 implementation of the jump patcher. (Unchanged)
 */
void JitBuffer_patch_jumps(JitBuffer *jit)
{
	for (size_t i = 0; i < jit->jump_patch_count; ++i) {
		JumpPatch *patch = &jit->jump_patches[i];
		if (patch->target_opcode_index >= jit->opcode_count) {
			fprintf(stderr,
				"Error: Invalid target opcode index %zu in jump patch.\n",
				patch->target_opcode_index);
			continue;
		}
		size_t target_buffer_offset =
			jit->opcode_addresses[patch->target_opcode_index];
		size_t jump_instruction_start_offset =
			patch->instruction_offset;
		size_t jump_instruction_end_offset =
			jump_instruction_start_offset + 6;
		intptr_t relative_offset_ptr =
			(intptr_t)target_buffer_offset -
			(intptr_t)jump_instruction_end_offset;
		if (relative_offset_ptr < INT32_MIN ||
		    relative_offset_ptr > INT32_MAX) {
			fprintf(stderr,
				"Error: Jump offset out of 32-bit range for patch %zu.\n",
				(size_t)relative_offset_ptr);
			continue;
		}
		int32_t relative_offset = (int32_t)relative_offset_ptr;
		size_t patch_location_offset =
			jump_instruction_start_offset + 2;
		if (patch_location_offset + sizeof(int32_t) > jit->size) {
			fprintf(stderr,
				"Error: Jump patch location %zu is out of bounds (buffer size %zu).\n",
				patch_location_offset, jit->size);
			continue;
		}
		memcpy(jit->buffer + patch_location_offset, &relative_offset,
		       sizeof(int32_t));
	}
}

/**
 * @brief Recursively compiles a function (or main body).
 * @param jit The JIT buffer.
 * @param code The full opcode vector.
 * @param start_pc The opcode index to start compiling.
 * @param end_pc The opcode index to stop compiling (exclusive).
 * @return The 64-bit memory address of the start of the compiled function.
 */
static uint64_t jit_compile_function(JitBuffer *jit, const OpcodeVector *code,
				     size_t start_pc, size_t end_pc)
{
	// Align to 16-bytes for function entry
	size_t alignment = 16 - (jit->size % 16);
	if (alignment != 16) {
		for (size_t i = 0; i < alignment; ++i) {
			if (!JitBuffer_push8(jit, 0x90))
				return 0; // NOP
		}
	}

	uint64_t function_start_addr = (uint64_t)(jit->buffer + jit->size);
	size_t pc = start_pc;

	if (!jit_push_reg(jit, REG_RBP))
		return 0;
	if (!jit_mov_reg_reg(jit, REG_RBP, REG_RSP))
		return 0;
	// Save callee-saved registers (RBX is our data pointer)
	if (!jit_push_reg(jit, REG_RBX))
		return 0;
	if (!jit_push_reg(jit, REG_R12))
		return 0; // Use R12 for return PC
	// other saved registers if needed

	while (pc < end_pc) {
		const opcode *op = &code->data[pc];
		JitBuffer_record_opcode_address(jit, pc);

		switch (op->op) {
		case op_add:
			if (!jit_add_mem8_imm8(jit, REG_RBX,
					       (uint8_t)(op->num & 0xff)))
				return 0;
			break;
		case op_sub:
			if (!jit_sub_mem8_imm8(jit, REG_RBX,
					       (uint8_t)(op->num & 0xff)))
				return 0;
			break;
		case op_addp:
			if (!jit_add_reg_imm32(jit, REG_RBX, op->num))
				return 0;
			break;
		case op_subp:
			if (!jit_sub_reg_imm32(jit, REG_RBX, op->num))
				return 0;
			break;
		case op_jt:
			if (!jit_mov_reg8_mem8(jit, REG_AL, REG_RBX))
				return 0;
			if (!jit_test_reg8_reg8(jit, REG_AL, REG_AL))
				return 0;
			if (!jit_jne(jit, op->num))
				return 0;
			break;
		case op_jf:
			if (!jit_mov_reg8_mem8(jit, REG_AL, REG_RBX))
				return 0;
			if (!jit_test_reg8_reg8(jit, REG_AL, REG_AL))
				return 0;
			if (!jit_je(jit, op->num))
				return 0;
			break;
		case op_in:
			if (!jit_mov_reg_imm64(jit, REG_RAX,
					       (uint64_t)(int (*)(void))getchar))
				return 0;
			if (!jit_call_reg(jit, REG_RAX))
				return 0;
			if (!jit_mov_mem8_reg8(jit, REG_RBX, REG_AL))
				return 0;
			break;
		case op_out:
			if (!jit_mov_reg_imm64(jit, REG_RAX,
					       (uint64_t)(int (*)(int))putchar))
				return 0;
#ifndef _WIN32
			if (!jit_movsx_reg_mem8(jit, REG_RDI, REG_RBX))
				return 0;
#else
			if (!jit_movsx_reg_mem8(jit, REG_RCX, REG_RBX))
				return 0;
#endif
			if (!jit_call_reg(jit, REG_RAX))
				return 0;
			break;
		case op_clear:
			if (!jit_mov_mem8_imm8(jit, REG_RBX, 0))
				return 0;
			break;

		case op_def_lambda: {
			// recursively compile the lambda body
			uint64_t lambda_addr = jit_compile_function(
				jit, code, pc + 1, op->num);
			if (lambda_addr == 0)
				return 0; // compilation failed

			// Emit code to call the runtime helper to push the closure
			// void jit_runtime_push_lambda(uint64_t jit_addr, uint32_t captured_p);
			// RDI = jit_addr, RSI = captured_p (RBX)
			if (!jit_mov_reg_imm64(jit, REG_RDI, lambda_addr))
				return 0;
			if (!jit_mov_reg_reg(jit, REG_RSI, REG_RBX))
				return 0; // captured_p
			if (!jit_mov_reg_imm64(
				    jit, REG_RAX,
				    (uint64_t)jit_runtime_push_lambda))
				return 0;
			if (!jit_call_reg(jit, REG_RAX))
				return 0;

			// Skip this function's body in the current compilation
			pc = op->num;
			continue; // Continue to next opcode after the lambda body
		}

		case op_ret:
			// Emit function epilogue and return
			if (!jit_pop_reg(jit, REG_R12))
				return 0;
			if (!jit_pop_reg(jit, REG_RBX))
				return 0;
			if (!jit_pop_reg(jit, REG_RBP))
				return 0;
			if (!jit_ret(jit))
				return 0;
			break;

		case op_call: {
			// Get the address after this call instruction group
			// mov r12, [rip + 0] ; 4c 8b 25 00 00 00 00 (REX.W + REX.R)
			if (!JitBuffer_push_bytes(jit,
						  (uint8_t[]){ 0x4c, 0x8b, 0x25,
							       0x00, 0x00, 0x00,
							       0x00 },
						  7))
				return 0;
			size_t ret_addr_patch_loc = jit->size;
			if (!JitBuffer_push64(jit, 0))
				return 0; // Placeholder address

			// Call pre_call runtime helper
			// Lambda jit_runtime_pre_call(uint32_t current_p, uint64_t return_pc);
			// RDI = current_p (RBX), RSI = return_pc (R12)
			if (!jit_mov_reg_reg(jit, REG_RDI, REG_RBX))
				return 0;
			if (!jit_mov_reg_reg(jit, REG_RSI, REG_R12))
				return 0;
			if (!jit_mov_reg_imm64(jit, REG_RAX,
					       (uint64_t)jit_runtime_pre_call))
				return 0;
			if (!jit_call_reg(jit, REG_RAX))
				return 0;
			// Lambda struct is returned in RAX (jit_addr) and RDX (captured_p)

			// Set up and make the call
			if (!jit_mov_reg_reg(jit, REG_RBX, REG_RDX))
				return 0; // Set new data ptr
			if (!jit_call_reg(jit, REG_RAX))
				return 0; // Call the lambda

			// Call post_ret runtime helper
			// uint32_t jit_runtime_post_ret(uint64_t* out_return_pc);
			// RDI = &return_pc (stack) -> Not needed, return PC is implicit
			if (!jit_rex_prefix(jit, true, false, false, false))
				return 0;
			if (!JitBuffer_push_bytes(
				    jit, (uint8_t[]){ 0x83, 0xec, 0x08 }, 3))
				return 0;
			// mov rdi, rsp
			if (!jit_mov_reg_reg(jit, REG_RDI, REG_RSP))
				return 0;
			if (!jit_mov_reg_imm64(jit, REG_RAX,
					       (uint64_t)jit_runtime_post_ret))
				return 0;
			if (!jit_call_reg(jit, REG_RAX))
				return 0;
			// add rsp, 8
			if (!jit_rex_prefix(jit, true, false, false, false))
				return 0;
			if (!JitBuffer_push_bytes(
				    jit, (uint8_t[]){ 0x83, 0xc4, 0x08 }, 3))
				return 0;

			if (!jit_mov_reg_reg(jit, REG_RBX, REG_RAX))
				return 0; // Restore data ptr

			// Patch the return address
			uint64_t ret_addr = (uint64_t)(jit->buffer + jit->size);
			memcpy(jit->buffer + ret_addr_patch_loc, &ret_addr,
			       sizeof(uint64_t));

			int32_t rel_offset = 8;
			memcpy(jit->buffer + ret_addr_patch_loc - 4,
			       &rel_offset, sizeof(int32_t));

			break;
		}
		}
		pc++; // Move to the next opcode
	}

	JitBuffer_record_opcode_address(jit,
					pc); // Record end-of-function address
	if (!jit_pop_reg(jit, REG_R12))
		return 0;
	if (!jit_pop_reg(jit, REG_RBX))
		return 0;
	if (!jit_pop_reg(jit, REG_RBP))
		return 0;
	if (!jit_ret(jit))
		return 0;

	return function_start_addr;
}

bool jit_exec_x86_64(const OpcodeVector *code)
{
    JitBuffer jit_mem;
    memset(&jit_mem, 0, sizeof(jit_mem));
    bool success = false;

    // allocate JIT buffer (+1 for the final "end of program" address)
    if (!JitBuffer_create(&jit_mem, 65536, code->size + 1)) {
        fprintf(stderr, "Failed to initialize JIT memory.\n");
        return false;
    }

    // init VM state
    memset(g_bf_mem, 0, sizeof(g_bf_mem));
    LambdaStack_init(&g_lambda_stack);
    CallStack_init(&g_call_stack);

    uint64_t main_func_addr =
        jit_compile_function(&jit_mem, code, 0, code->size);
    if (main_func_addr == 0) {
        fprintf(stderr, "JIT compilation failed.\n");
        goto error;
    }

    // Set data pointer for the main function and call it
    uint64_t start_addr = (uint64_t)(jit_mem.buffer + jit_mem.size);
    if (!jit_mov_reg_imm64(&jit_mem, REG_RBX, (uint64_t)g_bf_mem))
        goto error;
    if (!jit_mov_reg_imm64(&jit_mem, REG_RAX, main_func_addr))
        goto error;
    if (!jit_call_reg(&jit_mem, REG_RAX))
        goto error;
    if (!jit_ret(&jit_mem))
        goto error;

    // patch jumps before making executable
    JitBuffer_patch_jumps(&jit_mem);

#ifdef _WIN32
    DWORD oldProtect;
    if (!VirtualProtect(jit_mem.buffer, jit_mem.size, PAGE_EXECUTE_READ, &oldProtect)) {
        perror("VirtualProtect failed");
        goto error;
    }
#else
    if (mprotect(jit_mem.buffer, jit_mem.size, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect failed");
        goto error;
    }
#endif

    void (*func)(void) = (void (*)(void))start_addr;
    func();

    success = true;

error:
    JitBuffer_destroy(&jit_mem);
    LambdaStack_free(&g_lambda_stack);
    CallStack_free(&g_call_stack);
    return success;
}