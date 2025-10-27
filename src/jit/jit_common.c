#include "jit_common.h"

static bool busX86Jit_add_jump_patch(JitBuffer *jit, size_t target_opcode_index,
				     uint8_t jump_type)
{
	if (jit->jump_patch_count >= jit->jump_patch_capacity) {
		size_t new_capacity = jit->jump_patch_capacity == 0 ?
					      8 :
					      jit->jump_patch_capacity * 2;
		JumpPatch *new_patches = (JumpPatch *)realloc(
			jit->jump_patches, new_capacity * sizeof(JumpPatch));
		if (!new_patches) {
			perror("Failed to reallocate jump patches");
			return false;
		}
		jit->jump_patches = new_patches;
		jit->jump_patch_capacity = new_capacity;
	}
	jit->jump_patches[jit->jump_patch_count].instruction_offset = jit->size;
	jit->jump_patches[jit->jump_patch_count].target_opcode_index =
		target_opcode_index;
	jit->jump_patches[jit->jump_patch_count].jump_type = jump_type;
	jit->jump_patch_count++;
	return true;
}

bool JitBuffer_create(JitBuffer *jit, size_t capacity, size_t num_opcodes)
{
	jit->capacity = capacity;
	jit->size = 0;
	jit->opcode_count = num_opcodes;
	jit->jump_patch_count = 0;
	jit->jump_patch_capacity = 0;
	jit->jump_patches = NULL;

#ifdef _WIN32
	jit->buffer = (uint8_t *)VirtualAlloc(
		NULL, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (jit->buffer == NULL) {
		perror("VirtualAlloc failed");
		return false;
	}
#else
	jit->buffer = (uint8_t *)mmap(NULL, capacity, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (jit->buffer == MAP_FAILED) {
		perror("mmap failed");
		jit->buffer = NULL;
		return false;
	}
#endif

	jit->opcode_addresses = (size_t *)calloc(num_opcodes, sizeof(size_t));
	if (jit->opcode_addresses == NULL) {
		perror("Failed to allocate opcode_addresses");
#ifdef _WIN32
		VirtualFree(jit->buffer, 0, MEM_RELEASE);
#else
		if (jit->buffer)
			munmap(jit->buffer, jit->capacity);
#endif
		jit->buffer = NULL;
		return false;
	}
	return true;
}

void JitBuffer_destroy(JitBuffer *jit)
{
	if (jit->buffer) {
#ifdef _WIN32
		VirtualFree(jit->buffer, 0, MEM_RELEASE);
#else
		munmap(jit->buffer, jit->capacity);
#endif
	}
	free(jit->opcode_addresses);
	free(jit->jump_patches);
	memset(jit, 0, sizeof(JitBuffer));
}

bool JitBuffer_exec(JitBuffer *jit)
{
	// Call the backend-specific patcher
	JitBuffer_patch_jumps(jit);

#ifdef _WIN32
	DWORD oldProtect;
	if (!VirtualProtect(jit->buffer, jit->size, PAGE_EXECUTE_READ,
			    &oldProtect)) {
		perror("VirtualProtect failed");
		return false;
	}
#else
	if (mprotect(jit->buffer, jit->size, PROT_READ | PROT_EXEC) == -1) {
		perror("mprotect failed");
		return false;
	}
#endif
	void (*func)(void) = (void (*)(void))jit->buffer;
	func();
	return true;
}

bool JitBuffer_push_bytes(JitBuffer *jit, const uint8_t *bytes, size_t count)
{
	if (jit->size + count > jit->capacity) {
		fprintf(stderr, "JIT buffer overflow\n");
		return false;
	}
	memcpy(jit->buffer + jit->size, bytes, count);
	jit->size += count;
	return true;
}

bool JitBuffer_push8(JitBuffer *jit, uint8_t val)
{
	return JitBuffer_push_bytes(jit, &val, 1);
}

bool JitBuffer_push32(JitBuffer *jit, uint32_t val)
{
	return JitBuffer_push_bytes(jit, (uint8_t *)&val, sizeof(uint32_t));
}

bool JitBuffer_push64(JitBuffer *jit, uint64_t val)
{
	return JitBuffer_push_bytes(jit, (uint8_t *)&val, sizeof(uint64_t));
}

bool JitBuffer_add_jump_patch(JitBuffer *jit, size_t target_opcode_index,
			      uint8_t jump_type)
{
	return busX86Jit_add_jump_patch(jit, target_opcode_index, jump_type);
}

void JitBuffer_record_opcode_address(JitBuffer *jit, size_t opcode_index)
{
	if (opcode_index < jit->opcode_count) {
		jit->opcode_addresses[opcode_index] = jit->size;
	} else {
		fprintf(stderr,
			"Error: Invalid opcode index %zu for recording address (max %zu)\n",
			opcode_index, jit->opcode_count - 1);
	}
}
