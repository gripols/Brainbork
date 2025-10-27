#include "jit_aarch64.h"
#include "jit_common.h"
#include "jit_runtime.h"
#include "vm.h"

static bool jit_movz_w(JitBuffer *jit, uint8_t rd, uint16_t imm)
{
	uint32_t insn = (0x52800000) | (0x2 << 21) | ((uint32_t)imm << 5) | rd;
	return JitBuffer_push32(jit, insn);
}
static bool jit_movk_w(JitBuffer *jit, uint8_t rd, uint16_t imm)
{
	uint32_t insn = (0x52800000) | (0x3 << 21) | (1 << 21) |
			((uint32_t)imm << 5) | rd;
	return JitBuffer_push32(jit, insn);
}
static bool jit_mov_reg_imm32(JitBuffer *jit, uint8_t rd, uint32_t imm)
{
	if (!jit_movz_w(jit, rd, (imm & 0xFFFF)))
		return false;
	if ((imm >> 16) != 0) {
		if (!jit_movk_w(jit, rd, (imm >> 16)))
			return false;
	}
	return true;
}
static bool jit_mov_reg_imm64(JitBuffer *jit, uint8_t rd, uint64_t imm)
{
	uint32_t movz_base = (0xD2800000) | rd;
	uint32_t movk_base = (0xF2800000) | rd;
	if (!JitBuffer_push32(jit,
			      movz_base | ((imm & 0xFFFF) << 5) | (0 << 21)))
		return false;
	if ((imm >> 16) != 0) {
		if (!JitBuffer_push32(
			    jit, movk_base | (((imm >> 16) & 0xFFFF) << 5) |
					 (1 << 21)))
			return false;
	}
	if ((imm >> 32) != 0) {
		if (!JitBuffer_push32(
			    jit, movk_base | (((imm >> 32) & 0xFFFF) << 5) |
					 (2 << 21)))
			return false;
	}
	if ((imm >> 48) != 0) {
		if (!JitBuffer_push32(
			    jit, movk_base | (((imm >> 48) & 0xFFFF) << 5) |
					 (3 << 21)))
			return false;
	}
	return true;
}
static bool jit_add_reg_reg(JitBuffer *jit, uint8_t rd, uint8_t rn, uint8_t rm)
{
	uint32_t insn = (0x8B000000) | (rm << 16) | (rn << 5) | rd;
	return JitBuffer_push32(jit, insn);
}
static bool jit_sub_reg_reg(JitBuffer *jit, uint8_t rd, uint8_t rn, uint8_t rm)
{
	uint32_t insn = (0xCB000000) | (rm << 16) | (rn << 5) | rd;
	return JitBuffer_push32(jit, insn);
}
static bool jit_add_reg_reg_w(JitBuffer *jit, uint8_t rd, uint8_t rn,
			      uint8_t rm)
{
	uint32_t insn = (0x0B000000) | (rm << 16) | (rn << 5) | rd;
	return JitBuffer_push32(jit, insn);
}
static bool jit_sub_reg_reg_w(JitBuffer *jit, uint8_t rd, uint8_t rn,
			      uint8_t rm)
{
	uint32_t insn = (0x4B000000) | (rm << 16) | (rn << 5) | rd;
	return JitBuffer_push32(jit, insn);
}
static bool jit_ldrb_reg_reg(JitBuffer *jit, uint8_t rt, uint8_t rn)
{
	uint32_t insn = (0x39400000) | (rn << 5) | rt;
	return JitBuffer_push32(jit, insn);
}
static bool jit_ldrsb_reg_reg(JitBuffer *jit, uint8_t rt, uint8_t rn)
{
	uint32_t insn = (0x39800000) | (rn << 5) | rt;
	return JitBuffer_push32(jit, insn);
}
static bool jit_strb_reg_reg(JitBuffer *jit, uint8_t rt, uint8_t rn)
{
	uint32_t insn = (0x39000000) | (rn << 5) | rt;
	return JitBuffer_push32(jit, insn);
}
static bool jit_blr_reg(JitBuffer *jit, uint8_t rn)
{
	uint32_t insn = (0xD63F0000) | (rn << 5);
	return JitBuffer_push32(jit, insn);
}
static bool jit_ret(JitBuffer *jit)
{
	uint32_t insn = (0xD65F03C0);
	return JitBuffer_push32(jit, insn);
}
static bool jit_stp_pre(JitBuffer *jit, uint8_t rt, uint8_t rt2,
			int8_t imm_div_8)
{
	uint32_t imm7 = (uint32_t)(imm_div_8 & 0x7F);
	uint32_t insn =
		(0xA9800000) | (imm7 << 15) | (rt2 << 10) | (31 << 5) | rt;
	return JitBuffer_push32(jit, insn);
}
static bool jit_ldp_post(JitBuffer *jit, uint8_t rt, uint8_t rt2,
			 int8_t imm_div_8)
{
	uint32_t imm7 = (uint32_t)(imm_div_8 & 0x7F);
	uint32_t insn =
		(0xA8C00000) | (imm7 << 15) | (rt2 << 10) | (31 << 5) | rt;
	return JitBuffer_push32(jit, insn);
}
static bool jit_mov_reg_sp(JitBuffer *jit, uint8_t rd)
{
	uint32_t insn = (0x910003FF) | rd;
	return JitBuffer_push32(jit, insn);
}
static bool jit_cbz_reg(JitBuffer *jit, uint8_t rt, size_t target_opcode_index)
{
	uint32_t insn = (0x34000000) | rt;
	if (!JitBuffer_add_jump_patch(jit, target_opcode_index, 0))
		return false; // 0 = cbz
	return JitBuffer_push32(jit, insn);
}
static bool jit_cbnz_reg(JitBuffer *jit, uint8_t rt, size_t target_opcode_index)
{
	uint32_t insn = (0x35000000) | rt;
	if (!JitBuffer_add_jump_patch(jit, target_opcode_index, 1))
		return false; // 1 = cbnz
	return JitBuffer_push32(jit, insn);
}
static bool jit_mov_reg_reg(JitBuffer *jit, uint8_t rd, uint8_t rn)
{
	// mov x{rd}, x{rn} (alias for orr x{rd}, xzr, x{rn})
	uint32_t insn = (0xAA0003E0) | (rn << 16) | rd;
	return JitBuffer_push32(jit, insn);
}
/**
 * @brief AArch64 implementation of the jump patcher.
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
		intptr_t relative_offset_ptr =
			(intptr_t)target_buffer_offset -
			(intptr_t)jump_instruction_start_offset;
		if (relative_offset_ptr % 4 != 0) {
			fprintf(stderr,
				"Error: Jump offset %ld is not 4-byte aligned.\n",
				(long)relative_offset_ptr);
			continue;
		}
		int32_t offset_div_4 = (int32_t)(relative_offset_ptr / 4);
		if (offset_div_4 < -0x40000 || offset_div_4 > 0x3FFFF) {
			fprintf(stderr,
				"Error: Jump offset %ld out of +/-1MB range for cbz/cbnz.\n",
				(long)relative_offset_ptr);
			continue;
		}
		uint32_t imm19_field = (uint32_t)(offset_div_4 & 0x7FFFF);
		uint32_t *patch_addr =
			(uint32_t *)(jit->buffer +
				     jump_instruction_start_offset);
		*patch_addr = (*patch_addr & 0xFF00001F) | (imm19_field << 5);
	}
}

/**
 * @brief Recursively compiles a function (or main body) for AArch64.
 * @return The 64-bit memory address of the start of the compiled function.
 */
static uint64_t jit_compile_function_aarch64(JitBuffer *jit,
					     const OpcodeVector *code,
					     size_t start_pc, size_t end_pc)
{
	/* Align to 16-bytes for function entry */
	size_t alignment = 16 - (jit->size % 16);
	if (alignment != 16) {
		// AArch64 NOP
		for (size_t i = 0; i < alignment / 4; ++i)
			JitBuffer_push32(jit, 0xD503201F);
	}

	uint64_t function_start_addr = (uint64_t)(jit->buffer + jit->size);
	size_t pc = start_pc;

	// x19 = data pointer, x20 = scratch/runtime arg
	// stp x29, x30, [sp, #-32]!  (Save FP, LR)
	if (!jit_stp_pre(jit, 29, 30, -4))
		goto error;
	// mov x29, sp
	if (!jit_mov_reg_sp(jit, 29))
		goto error;
	// stp x19, x20, [sp, #16]   (Save callee-saved x19, x20)
	uint32_t stp_x19_x20 =
		(0xA9010000) | (2 << 15) | (20 << 10) | (31 << 5) | 19;
	if (!JitBuffer_push32(jit, stp_x19_x20))
		goto error;

	while (pc < end_pc) {
		const opcode *op = &code->data[pc];
		JitBuffer_record_opcode_address(jit, pc);

		switch (op->op) {
		case op_add:
			if (!jit_ldrb_reg_reg(jit, 0, 19))
				goto error;
			if (!jit_mov_reg_imm32(jit, 1, op->num))
				goto error;
			if (!jit_add_reg_reg_w(jit, 0, 0, 1))
				goto error;
			if (!jit_strb_reg_reg(jit, 0, 19))
				goto error;
			break;
		case op_sub:
			if (!jit_ldrb_reg_reg(jit, 0, 19))
				goto error;
			if (!jit_mov_reg_imm32(jit, 1, op->num))
				goto error;
			if (!jit_sub_reg_reg_w(jit, 0, 0, 1))
				goto error;
			if (!jit_strb_reg_reg(jit, 0, 19))
				goto error;
			break;
		case op_addp:
			if (!jit_mov_reg_imm32(jit, 0, op->num))
				goto error;
			if (!jit_add_reg_reg(jit, 19, 19, 0))
				goto error;
			break;
		case op_subp:
			if (!jit_mov_reg_imm32(jit, 0, op->num))
				goto error;
			if (!jit_sub_reg_reg(jit, 19, 19, 0))
				goto error;
			break;
		case op_jt:
			if (!jit_ldrb_reg_reg(jit, 0, 19))
				goto error;
			if (!jit_cbnz_reg(jit, 0, op->num))
				goto error;
			break;
		case op_jf:
			if (!jit_ldrb_reg_reg(jit, 0, 19))
				goto error;
			if (!jit_cbz_reg(jit, 0, op->num))
				goto error;
			break;
		case op_in:
			if (!jit_mov_reg_imm64(jit, 0, (uint64_t)getchar))
				goto error;
			if (!jit_blr_reg(jit, 0))
				goto error;
			if (!jit_strb_reg_reg(jit, 0, 19))
				goto error;
			break;
		case op_out:
			if (!jit_ldrsb_reg_reg(jit, 0, 19))
				goto error;
			if (!jit_mov_reg_imm64(jit, 1, (uint64_t)putchar))
				goto error;
			if (!jit_blr_reg(jit, 1))
				goto error;
			break;
		case op_clear:
			if (!jit_mov_reg_imm32(jit, 0, 0))
				goto error;
			if (!jit_strb_reg_reg(jit, 0, 19))
				goto error;
			break;

		case op_def_lambda: {
			uint64_t lambda_addr = jit_compile_function_aarch64(
				jit, code, pc + 1, op->num);
			if (lambda_addr == 0)
				return 0;

			// Emit call to runtime helper
			// void jit_runtime_push_lambda(uint64_t jit_addr, uint32_t captured_p);
			// x0 = jit_addr, w1 = captured_p (x19)
			if (!jit_mov_reg_imm64(jit, 0, lambda_addr))
				goto error;
			if (!jit_mov_reg_reg(jit, 1, 19))
				return 0; // w1 <- x19
			if (!jit_mov_reg_imm64(
				    jit, 2, (uint64_t)jit_runtime_push_lambda))
				return 0;
			if (!jit_blr_reg(jit, 2))
				return 0;

			// Skip body
			pc = op->num;
			continue;
		}

		case op_ret:
			// ldp x19, x20, [sp, #16]
			if (!JitBuffer_push32(jit, (0xA9410000) | (2 << 15) |
							   (20 << 10) |
							   (31 << 5) | 19))
				return 0;
			// ldp x29, x30, [sp], #32
			if (!jit_ldp_post(jit, 29, 30, 4))
				return 0;
			if (!jit_ret(jit))
				return 0;
			break;

		case op_call: {
			// mov x20, <placeholder>
			if (!jit_mov_reg_imm64(jit, 20, 0xDEADBEEFCAFEBABE))
				return 0;
			size_t ret_addr_patch_loc =
				jit->size - 8; // Start of the 64-bit imm

			// Lambda jit_runtime_pre_call(uint32_t current_p, uint64_t return_pc);
			// w0 = current_p (x19), x1 = return_pc (x20)
			if (!jit_mov_reg_reg(jit, 0, 19))
				return 0; // w0 <- x19
			if (!jit_mov_reg_reg(jit, 1, 20))
				return 0; // x1 <- x20
			if (!jit_mov_reg_imm64(jit, 2,
					       (uint64_t)jit_runtime_pre_call))
				return 0;
			if (!jit_blr_reg(jit, 2))
				return 0;
			// Lambda struct is returned in x0 (jit_addr) and x1 (captured_p)

			if (!jit_mov_reg_reg(jit, 19, 1))
				return 0; // x19 <- x1 (new data ptr)
			if (!jit_blr_reg(jit, 0))
				return 0; // call lambda

			// uint32_t jit_runtime_post_ret(uint64_t* out_return_pc);
			// x0 will have our *out_return_pc (not needed)
			// w0 will have the saved_p
			if (!jit_mov_reg_imm64(jit, 2,
					       (uint64_t)jit_runtime_post_ret))
				return 0;
			if (!jit_blr_reg(jit, 2))
				return 0;
			// w0 now holds the saved_p
			if (!jit_mov_reg_reg(jit, 19, 0))
				return 0; // x19 <- w0

			// Patch the return address
			uint64_t ret_addr = (uint64_t)(jit->buffer + jit->size);
			memcpy(jit->buffer + ret_addr_patch_loc, &ret_addr,
			       sizeof(uint64_t));
			break;
		}
		}
		pc++;
	}

	JitBuffer_record_opcode_address(jit, pc);
	// ldp x19, x20, [sp, #16]
	if (!JitBuffer_push32(jit, (0xA9410000) | (2 << 15) | (20 << 10) |
					   (31 << 5) | 19))
		return 0;
	// ldp x29, x30, [sp], #32
	if (!jit_ldp_post(jit, 29, 30, 4))
		return 0;
	if (!jit_ret(jit))
		return 0;

	return function_start_addr;

error:
	return 0;
}

bool jit_exec_aarch64(const OpcodeVector *code)
{
	JitBuffer jit_mem;
	memset(&jit_mem, 0, sizeof(jit_mem));
	bool success = false;

	if (!JitBuffer_create(&jit_mem, 65536, code->size + 1)) {
		fprintf(stderr, "Failed to initialize JIT memory.\n");
		return false;
	}

	// init VM state
	memset(g_bf_mem, 0, sizeof(g_bf_mem));
	LambdaStack_init(&g_lambda_stack);
	CallStack_init(&g_call_stack);

	uint64_t main_func_addr =
		jit_compile_function_aarch64(&jit_mem, code, 0, code->size);
	if (main_func_addr == 0) {
		fprintf(stderr, "JIT compilation failed.\n");
		goto error;
	}

	uint64_t start_addr = (uint64_t)(jit_mem.buffer + jit_mem.size);
	// mov x19, g_bf_mem
	if (!jit_mov_reg_imm64(&jit_mem, 19, (uint64_t)g_bf_mem))
		goto error;
	// mov x0, main_func_addr
	if (!jit_mov_reg_imm64(&jit_mem, 0, main_func_addr))
		goto error;
	// blr x0
	if (!jit_blr_reg(&jit_mem, 0))
		goto error;
	// ret
	if (!jit_ret(&jit_mem))
		goto error;

	JitBuffer_patch_jumps(&jit_mem);

	// mprotect is the same for both
	if (mprotect(jit_mem.buffer, jit_mem.size, PROT_READ | PROT_EXEC) ==
	    -1) {
		perror("mprotect failed");
		goto error;
	}

	__builtin___clear_cache((char *)jit_mem.buffer,
				(char *)(jit_mem.buffer + jit_mem.size));

	void (*func)(void) = (void (*)(void))start_addr;
	func();

	success = true;

error:
	JitBuffer_destroy(&jit_mem);
	LambdaStack_free(&g_lambda_stack);
	CallStack_free(&g_call_stack);
	return success;
}
