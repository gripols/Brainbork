#ifndef JIT_COMMON_H
#define JIT_COMMON_H

#include "brainfork.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

typedef struct {
	size_t instruction_offset; // Offset in 'buffer' where the jump instruction starts
	size_t target_opcode_index; // The opcode index this jump targets
	uint8_t jump_type; // 0 = JE, 1 = JNE (or other backend-specific types)
} JumpPatch;

typedef struct JitBuffer {
	uint8_t *buffer; // The executable memory buffer
	size_t capacity; // Total allocated size of buffer
	size_t size; // Current size of generated code

	size_t *opcode_addresses; // Map: opcode_index -> buffer_offset
	size_t opcode_count;

	JumpPatch *jump_patches;
	size_t jump_patch_count;
	size_t jump_patch_capacity;
} JitBuffer;

bool JitBuffer_create(JitBuffer *jit, size_t capacity, size_t num_opcodes);
void JitBuffer_destroy(JitBuffer *jit);
bool JitBuffer_exec(JitBuffer *jit);

bool JitBuffer_push_bytes(JitBuffer *jit, const uint8_t *bytes, size_t count);
bool JitBuffer_push8(JitBuffer *jit, uint8_t val);
bool JitBuffer_push32(JitBuffer *jit, uint32_t val);
bool JitBuffer_push64(JitBuffer *jit, uint64_t val);

bool JitBuffer_add_jump_patch(JitBuffer *jit, size_t target_opcode_index,
			      uint8_t jump_type);
void JitBuffer_record_opcode_address(JitBuffer *jit, size_t opcode_index);

/**
 * @brief A generic jump patcher.
 * * This function is architecture-specific and must be implemented by the backend.
 * It iterates over `jump_patches` and patches the 'buffer' with correct relative offsets.
 */
void JitBuffer_patch_jumps(JitBuffer *jit);

#endif // JIT_COMMON_H
