#include "jit.h"
#include "brainfork.h"

#if defined(__x86_64__) || defined(_M_X64)
#include "jit_x86_64.h"
#elif defined(__aarch64__)
#include "jit_aarch64.h"
#endif

bool jit(const OpcodeVector *code)
{
#if defined(__x86_64__) || defined(_M_X64)
	return jit_exec_x86_64(code);
#elif defined(__aarch64__)
	return jit_exec_aarch64(code);
#else
	// Placeholder for other architectures
	fprintf(stderr,
		"JIT compilation is not supported on this architecture.\n");
	return false;
#endif
}
