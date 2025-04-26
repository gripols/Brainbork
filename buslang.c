#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

typedef struct {
    uint8_t op;
    uint32_t num;
    size_t offset; // check if needed 
} opcode;

typedef struct {
    opcode* data;
    size_t size;
    size_t capacity;
} OpcodeVector;

void OpcodeVector_init(OpcodeVector* vec) {
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

void OpcodeVector_push_back(OpcodeVector* vec, opcode val) {
    if (vec->size >= vec->capacity) {
        size_t new_capacity = vec->capacity == 0 ? 8 : vec->capacity * 2;
        opcode* new_data = (opcode*)realloc(vec->data, new_capacity * sizeof(opcode));
        if (!new_data) {
            perror("Failed to reallocate opcode vector");
            exit(EXIT_FAILURE);
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = val;
}

void OpcodeVector_free(OpcodeVector* vec) {
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

// --- Stack Implementation for size_t ---
typedef struct {
    size_t* data;
    size_t size;
    size_t capacity;
} SizeTStack;

void SizeTStack_init(SizeTStack* stk) {
    stk->data = NULL;
    stk->size = 0;
    stk->capacity = 0;
}

int SizeTStack_empty(const SizeTStack* stk) {
    return stk->size == 0;
}

void SizeTStack_push(SizeTStack* stk, size_t val) {
    if (stk->size >= stk->capacity) {
        size_t new_capacity = stk->capacity == 0 ? 8 : stk->capacity * 2;
        size_t* new_data = (size_t*)realloc(stk->data, new_capacity * sizeof(size_t));
        if (!new_data) {
            perror("Failed to reallocate size_t stack");
            exit(EXIT_FAILURE);
        }
        stk->data = new_data;
        stk->capacity = new_capacity;
    }
    stk->data[stk->size++] = val;
}

void SizeTStack_pop(SizeTStack* stk) {
    if (stk->size > 0) {
        stk->size--;
    }
}

size_t SizeTStack_top(const SizeTStack* stk) {
    if (stk->size > 0) {
        return stk->data[stk->size - 1];
    }
    // exit here for safety as to match the original exit behavior on empty stack access during scanning
    fprintf(stderr, "Error: Accessing top of empty stack.\n");
    exit(EXIT_FAILURE);
}

void SizeTStack_free(SizeTStack* stk) {
    free(stk->data);
    stk->data = NULL;
    stk->size = 0;
    stk->capacity = 0;
}


// hold jump patch information
typedef struct {
    size_t instruction_offset; // Offset of the jump instruction start in the buffer
    size_t target_opcode_index; // Index of the target opcode in the original code vector
    uint8_t jump_type;         // 0 for JE (op_jf), 1 for JNE (op_jt)
} JumpPatch;

// JIT compiler state
typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t size;
    size_t* opcode_addresses; // Maps opcode index to buffer offset
    size_t opcode_count;      // Total number of opcodes
    JumpPatch* jump_patches;
    size_t jump_patch_count;
    size_t jump_patch_capacity;
} busX86Jit;

// helper: add jump patch info
void busX86Jit_add_jump_patch(busX86Jit* jit, size_t target_opcode_index, uint8_t jump_type) {
    if (jit -> jump_patch_count >= jit -> jump_patch_capacity) {
        size_t new_capacity = jit -> jump_patch_capacity == 0 ? 8 : jit -> jump_patch_capacity * 2;
        JumpPatch* new_patches = (JumpPatch*)realloc(jit->jump_patches, new_capacity * sizeof(JumpPatch));
        if (!new_patches) {
            perror("Failed to reallocate jump patches");
            exit(EXIT_FAILURE);
        }
        jit->jump_patches = new_patches;
        jit->jump_patch_capacity = new_capacity;
    }
    // Store the offset where the *relative address* will be written (after the opcode)
    jit->jump_patches[jit->jump_patch_count].instruction_offset = jit->size;
    jit->jump_patches[jit->jump_patch_count].target_opcode_index = target_opcode_index;
    jit->jump_patches[jit->jump_patch_count].jump_type = jump_type;
    jit->jump_patch_count++;
}

// Initialize JIT state
int busX86Jit_create(busX86Jit* jit, size_t capacity, size_t num_opcodes) {
    jit->capacity = capacity;
    jit->size = 0;
    jit->opcode_count = num_opcodes;
    jit->jump_patch_count = 0;
    jit->jump_patch_capacity = 0;
    jit->jump_patches = NULL;

#ifdef _WIN32
    jit->buffer = (uint8_t*)VirtualAlloc(NULL, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (jit->buffer == NULL) {
        perror("VirtualAlloc failed");
        return 0; // Failure
    }
#else
    jit->buffer = (uint8_t*)mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (jit->buffer == MAP_FAILED) {
        perror("mmap failed");
        jit->buffer = NULL; // Ensure buffer is NULL on failure
        return 0; // Failure
    }
#endif

    jit->opcode_addresses = (size_t*)calloc(num_opcodes, sizeof(size_t));
    if (jit->opcode_addresses == NULL) {
        perror("Failed to allocate opcode_addresses");
#ifdef _WIN32
        VirtualFree(jit->buffer, 0, MEM_RELEASE);
#else
        if (jit->buffer) munmap(jit->buffer, jit->capacity);
#endif
        jit->buffer = NULL;
        return 0; // Failure
    }

    return 1; // Success
}

// Destroy JIT state
void busX86Jit_destroy(busX86Jit* jit) {
    if (jit->buffer) {
#ifdef _WIN32
        VirtualFree(jit->buffer, 0, MEM_RELEASE);
#else
        munmap(jit->buffer, jit->capacity);
#endif
        jit->buffer = NULL;
    }
    free(jit->opcode_addresses);
    jit->opcode_addresses = NULL;
    free(jit->jump_patches);
    jit->jump_patches = NULL;
    jit->capacity = 0;
    jit->size = 0;
    jit->opcode_count = 0;
    jit->jump_patch_count = 0;
    jit->jump_patch_capacity = 0;
}

// Append bytes to the JIT buffer
void busX86Jit_push_bytes(busX86Jit* jit, const uint8_t* bytes, size_t count) {
    if (jit->size + count > jit->capacity) {
        fprintf(stderr, "JIT buffer overflow\n");
        exit(EXIT_FAILURE);
    }
    memcpy(jit->buffer + jit->size, bytes, count);
    jit->size += count;
}

void busX86Jit_push8(busX86Jit* jit, uint8_t val) {
    busX86Jit_push_bytes(jit, &val, 1);
}

void busX86Jit_push32(busX86Jit* jit, uint32_t val) {
    busX86Jit_push_bytes(jit, (uint8_t*)&val, sizeof(uint32_t));
}

void busX86Jit_push64(busX86Jit* jit, uint64_t val) {
    busX86Jit_push_bytes(jit, (uint8_t*)&val, sizeof(uint64_t));
}

void busX86Jit_record_opcode_address(busX86Jit* jit, size_t opcode_index) {
    if (opcode_index < jit->opcode_count) {
        jit->opcode_addresses[opcode_index] = jit->size;
    } else {
         fprintf(stderr, "Error: Invalid opcode index %zu for recording address (max %zu)\n", opcode_index, jit->opcode_count -1);
         // Decide how to handle this. Maybe exit? Idfk for now just print error.
    }
}

// Generate JE instruction
void busX86Jit_je(busX86Jit* jit, size_t target_opcode_index) {
    // JE rel32: 0x0F 0x84 imm32
    uint8_t je_op[] = {0x0f, 0x84, 0x00, 0x00, 0x00, 0x00}; // Placeholder offset
    busX86Jit_add_jump_patch(jit, target_opcode_index, 0); // 0 for JE
    busX86Jit_push_bytes(jit, je_op, sizeof(je_op));
}

// Generate JNE instruction
void busX86Jit_jne(busX86Jit* jit, size_t target_opcode_index) {
    // JNE rel32: 0x0F 0x85 imm32
    uint8_t jne_op[] = {0x0f, 0x85, 0x00, 0x00, 0x00, 0x00}; // Placeholder offset
    busX86Jit_add_jump_patch(jit, target_opcode_index, 1); // 1 for JNE
    busX86Jit_push_bytes(jit, jne_op, sizeof(jne_op));
}

// Patch jumps after all code is generated
void busX86Jit_patch_jumps(busX86Jit* jit) {
    for (size_t i = 0; i < jit->jump_patch_count; ++i) {
        JumpPatch* patch = &jit->jump_patches[i];

        if (patch->target_opcode_index >= jit->opcode_count) {
             fprintf(stderr, "Error: Invalid target opcode index %zu in jump patch.\n", patch->target_opcode_index);
             continue; // Skip this patch
        }

        size_t target_buffer_offset = jit->opcode_addresses[patch->target_opcode_index];
        size_t jump_instruction_start_offset = patch->instruction_offset;
        // relative offset is calculated from the next instruction's address
        size_t jump_instruction_end_offset = jump_instruction_start_offset + 6; // JE/JNE rel32 is 6 bytes (0x0F 0x8? imm32)

        intptr_t relative_offset_ptr = (intptr_t)target_buffer_offset - (intptr_t)jump_instruction_end_offset;

        // Check if the offset fits within 32 bits
        if (relative_offset_ptr < INT32_MIN || relative_offset_ptr > INT32_MAX) {
             fprintf(stderr, "Error: Jump offset out of 32-bit range for patch %zu.\n", i);
             // TODO: Handle error - maybe exit or skip?
             continue;
        }
        int32_t relative_offset = (int32_t)relative_offset_ptr;


        // The offset (imm32) starts 2 bytes into the instruction (after 0x0F 0x8?)
        size_t patch_location_offset = jump_instruction_start_offset + 2;

        if (patch_location_offset + sizeof(int32_t) > jit->size) {
             fprintf(stderr, "Error: Jump patch location %zu is out of bounds (buffer size %zu).\n", patch_location_offset, jit->size);
             continue; // fuck this patch
        }

        memcpy(jit->buffer + patch_location_offset, &relative_offset, sizeof(int32_t));
    }
}


// Execute the JIT code
void busX86Jit_exec(busX86Jit* jit) {
    // Patch the jumps first
    busX86Jit_patch_jumps(jit);

    // Change memory protection to executable
#ifdef _WIN32
    DWORD oldProtect;
    if (!VirtualProtect(jit->buffer, jit->size, PAGE_EXECUTE_READ, &oldProtect)) {
        perror("VirtualProtect failed");
        exit(EXIT_FAILURE);
    }
#else
    if (mprotect(jit->buffer, jit->size, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect failed");
        exit(EXIT_FAILURE);
    }
#endif

    // Cast buffer to function pointer and execute
    void (*func)(void) = (void (*)(void))jit->buffer;
    func();

    // (Optional) Restore memory protection
#ifdef _WIN32
    // It might be desirable to restore protection, but requires careful handling
    // if the JIT code needs to be executed again. For simplicity, we omit it here.
    // VirtualProtect(jit->buffer, jit->size, oldProtect, &oldProtect);
#else
    // mprotect(jit->buffer, jit->size, PROT_READ | PROT_WRITE);
#endif
}


// --- Logic ---

uint8_t buff[0x20000];

enum op {
    op_add,
    op_sub,
    op_addp,
    op_subp,
    op_jt,
    op_jf,
    op_in,
    op_out
};

// struct opcode is defined above with the vector implementation

OpcodeVector scanner(const char* s) {
    OpcodeVector code;
    SizeTStack stk;
    OpcodeVector_init(&code);
    SizeTStack_init(&stk);

    uint32_t cnt = 0;
    int line = 0; // Line number for error reporting
    size_t len = strlen(s);

    for (size_t i = 0; i < len;) {
        // Skip whitespace
        while (i < len && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) {
            if (s[i] == '\n') ++line;
            ++i;
        }
        if (i >= len) break;

        // Match words
        if (strncmp(&s[i], "ROUTE", 5) == 0) {
            cnt = 0;
            while (i < len && strncmp(&s[i], "ROUTE", 5) == 0) {
                ++cnt;
                i += 5;
                while (i < len && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
            }
            opcode op = {op_add, cnt & 0xff, 3};
            OpcodeVector_push_back(&code, op);
        } else if (strncmp(&s[i], "102", 3) == 0) {
            cnt = 0;
            while (i < len && strncmp(&s[i], "102", 3) == 0) {
                ++cnt;
                i += 3;
                while (i < len && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
            }
            opcode op = {op_sub, cnt & 0xff, 3};
            OpcodeVector_push_back(&code, op);
        } else if (strncmp(&s[i], "MARKHAM", 7) == 0) {
            cnt = 0;
            while (i < len && strncmp(&s[i], "MARKHAM", 7) == 0) {
                ++cnt;
                i += 7;
                while (i < len && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
            }
            opcode op = {op_addp, cnt, 7};
            OpcodeVector_push_back(&code, op);
        } else if (strncmp(&s[i], "ROAD", 4) == 0) {
            cnt = 0;
            while (i < len && strncmp(&s[i], "ROAD", 4) == 0) {
                ++cnt;
                i += 4;
                while (i < len && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
            }
            opcode op = {op_subp, cnt, 7};
            OpcodeVector_push_back(&code, op);
        } else if (strncmp(&s[i], "SOUTHBOUND", 10) == 0) {
            SizeTStack_push(&stk, code.size);
            opcode op = {op_jf, 0, 10}; // Placeholder
            OpcodeVector_push_back(&code, op);
            i += 10;
        } else if (strncmp(&s[i], "TOWARDS", 7) == 0) {
            if (SizeTStack_empty(&stk)) {
                printf("empty stack at line %d\n", line);
                exit(-1);
            }
            size_t jf_idx = SizeTStack_top(&stk);
            SizeTStack_pop(&stk);
            code.data[jf_idx].num = (uint32_t)code.size;
            opcode op = {op_jt, (uint32_t)jf_idx, 10};
            OpcodeVector_push_back(&code, op);
            i += 7;
        } else if (strncmp(&s[i], "WARDEN", 6) == 0) {
            opcode op = {op_in, 0, 14};
            OpcodeVector_push_back(&code, op);
            i += 6;
        } else if (strncmp(&s[i], "STATION", 7) == 0) {
            opcode op = {op_out, 0, 15};
            OpcodeVector_push_back(&code, op);
            i += 7;
        } else {
            printf("Unknown keyword at line %d: starting at '%c'\n", line, s[i]);
            exit(-1);
        }
    }

    if (!SizeTStack_empty(&stk)) {
        printf("lack STATION\n");
        exit(-1);
    }

    SizeTStack_free(&stk);
    return code;
}

void interpreter(const OpcodeVector* code) {
    clock_t begin = clock(); // Use clock() for timing
    memset(buff, 0, sizeof(buff));
    uint32_t p = 0; // Data pointer
    size_t pc = 0; // Program counter

    while(pc < code->size) {
        const opcode* op = &code->data[pc];
        switch(op->op) {
            case op_add:  buff[p] += op->num; pc++; break;
            case op_sub:  buff[p] -= op->num; pc++; break;
            case op_addp: p += op->num; pc++; break;
            case op_subp: p -= op->num; pc++; break;
            case op_jt:   if(buff[p]) pc = op->num; else pc++; break; // Jump if non-zero
            case op_jf:   if(!buff[p]) pc = op->num; else pc++; break; // Jump if zero
            case op_in:   buff[p] = getchar(); pc++; break;
            case op_out:  putchar(buff[p]); pc++; break;
            default: // Should not happen
                fprintf(stderr, "Unknown opcode: %d\n", op->op);
                pc++;
                break;
        }
    }

    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("\ninterpreter time usage: %fs\n", time_spent); // Use printf and %f
}

void jit(const OpcodeVector* code) {
    busX86Jit jit_mem;
    // Initialize JIT compiler state
    if (!busX86Jit_create(&jit_mem, 65536, code->size)) {
        fprintf(stderr, "Failed to initialize JIT memory.\n");
        return;
    }

    memset(buff, 0, sizeof(buff));

    /* set stack and base pointer */
    { uint8_t bytes[] = {0x55}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // pushq %rbp
    { uint8_t bytes[] = {0x48, 0x89, 0xe5}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // mov %rsp,%rbp

    /* save register context */
    { uint8_t bytes[] = {0x57}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // pushq %rdi
    { uint8_t bytes[] = {0x56}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // pushq %rsi
    { uint8_t bytes[] = {0x53}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // pushq %rbx
    { uint8_t bytes[] = {0x52}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // pushq %rdx
    { uint8_t bytes[] = {0x51}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // pushq %rcx
    { uint8_t bytes[] = {0x50}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // pushq %rax

    /* set bf machine's paper pointer */
    { uint8_t bytes[] = {0x48, 0xbb}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // movq $buff,%rbx
    busX86Jit_push64(&jit_mem, (uint64_t)buff);

    // Generate code for each opcode
    for(size_t i = 0; i < code->size; ++i) {
        const opcode* op = &code->data[i];
        busX86Jit_record_opcode_address(&jit_mem, i); // Record address before generating code for this opcode

        switch(op->op) {
            case op_add:
                { uint8_t bytes[] = {0x80, 0x03, (uint8_t)(op->num & 0xff)}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // addb $op.num,(%rbx)
                break;
            case op_sub:
                { uint8_t bytes[] = {0x80, 0x2b, (uint8_t)(op->num & 0xff)}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // subb $op.num,(%rbx)
                break;
            case op_addp:
                { uint8_t bytes[] = {0x48, 0x81, 0xc3}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // add $op.num,%rbx
                busX86Jit_push32(&jit_mem, op->num);
                break;
            case op_subp:
                { uint8_t bytes[] = {0x48, 0x81, 0xeb}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // sub $op.num,%rbx
                busX86Jit_push32(&jit_mem, op->num);
                break;
            case op_jt: // if(byte at ptr != 0) jump to matching '['
                { uint8_t bytes[] = {0x8a, 0x03}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // mov (%rbx),%al
                { uint8_t bytes[] = {0x84, 0xc0}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // test %al,%al
                busX86Jit_jne(&jit_mem, op->num); // Jump if NOT equal (non-zero) to the target opcode index
                break;
            case op_jf: // if(byte at ptr == 0) jump to matching ']'
                { uint8_t bytes[] = {0x8a, 0x03}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // mov (%rbx),%al
                { uint8_t bytes[] = {0x84, 0xc0}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // test %al,%al
                busX86Jit_je(&jit_mem, op->num); // Jump if equal (zero) to the target opcode index
                break;
            case op_in:
                { uint8_t bytes1[] = {0x48, 0xb8}; busX86Jit_push_bytes(&jit_mem, bytes1, sizeof(bytes1)); } // movabs $getchar,%rax
                busX86Jit_push64(&jit_mem, (uint64_t)(int (*)(void))getchar); // Cast getchar appropriately
                { uint8_t bytes2[] = {0xff, 0xd0}; busX86Jit_push_bytes(&jit_mem, bytes2, sizeof(bytes2)); } // callq *%rax
                { uint8_t bytes3[] = {0x88, 0x03}; busX86Jit_push_bytes(&jit_mem, bytes3, sizeof(bytes3)); } // mov %al,(%rbx) - Store low byte of result
                break;
            case op_out:
                { uint8_t bytes1[] = {0x48, 0xb8}; busX86Jit_push_bytes(&jit_mem, bytes1, sizeof(bytes1)); } // movabs $putchar,%rax
                busX86Jit_push64(&jit_mem, (uint64_t)(int (*)(int))putchar); // Cast putchar appropriately
#ifndef _WIN32
                // System V ABI (Linux, macOS): first arg in %rdi
                { uint8_t bytes2[] = {0x0f, 0xbe, 0x3b}; busX86Jit_push_bytes(&jit_mem, bytes2, sizeof(bytes2)); } // movsbl (%rbx),%edi (zero-extends to rdi)
#else
                // Windows x64 ABI: first arg in %rcx
                { uint8_t bytes2[] = {0x0f, 0xbe, 0x0b}; busX86Jit_push_bytes(&jit_mem, bytes2, sizeof(bytes2)); } // movsbl (%rbx),%ecx (zero-extends to rcx)
#endif
                { uint8_t bytes3[] = {0xff, 0xd0}; busX86Jit_push_bytes(&jit_mem, bytes3, sizeof(bytes3)); } // callq *%rax
                break;
        }
    }

    // Ensure the last opcode address is recorded if the loop finishes
    // This might be needed if a jump targets the position *after* the last instruction.
    // busX86Jit_record_opcode_address(&jit_mem, code->size); // Add a conceptual address for "end of code"

    // Absolutely treacherous but here we go
    { uint8_t bytes[] = {0x58}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // popq %rax
    { uint8_t bytes[] = {0x59}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // popq %rcx
    { uint8_t bytes[] = {0x5a}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // popq %rdx
    { uint8_t bytes[] = {0x5b}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // popq %rbx
    { uint8_t bytes[] = {0x5e}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // popq %rsi
    { uint8_t bytes[] = {0x5f}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // popq %rdi
    { uint8_t bytes[] = {0x5d}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // popq %rbp
    { uint8_t bytes[] = {0xc3}; busX86Jit_push_bytes(&jit_mem, bytes, sizeof(bytes)); } // retq

    clock_t begin = clock();
    busX86Jit_exec(&jit_mem); // Execute the generated code
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("\njit-compiler time usage: %fs\n", time_spent);

    busX86Jit_destroy(&jit_mem); // Clean up JIT memory
}

void usage() {
    printf("usage:\n" // Use printf
           "  jit [options] <filename>\n\n"
           "options:\n"
           "  -i | interpreter mode\n"
           "  -j | JIT compiler mode\n");
}

char* read_file_to_string(const char* filename) {
    FILE* file = fopen(filename, "rb"); // Open in binary read mode
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size < 0) { // Check for ftell error
        fclose(file);
        return NULL;
    }
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1); // +1 for null terminator
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        fclose(file);
        free(buffer);
        return NULL;
    }

    buffer[file_size] = '\0'; // Null terminate the string
    fclose(file);
    return buffer;
}


int main(int argc, const char* argv[]) {
    if(argc==1){
        usage();
        return 0;
    }

    int interpreter_mode = 0; // fuck it int as bool
    int jit_compiler_mode = 0; // ditto
    int filename_index = -1;

    for(int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "-i") == 0) {
            interpreter_mode = 1;
        } else if(strcmp(argv[i], "-j") == 0) {
            jit_compiler_mode = 1;
        } else if(argv[i][0] != '-') {
            if (filename_index != -1) {
                 printf("error: multiple filenames provided.\n\n");
                 usage();
                 return -1;
            }
            filename_index = i;
        } else {
            printf("error argument \"%s\"\n\n", argv[i]);
            usage();
            return -1;
        }
    }

    if(!interpreter_mode && !jit_compiler_mode) {
        printf("please choose an interpreter or JIT-compiler\n\n");
        usage();
        return -1;
    }

    if(filename_index < 0) {
        printf("no input file\n\n");
        usage();
        return -1;
    }

    char* file_content = read_file_to_string(argv[filename_index]);
    if (!file_content) {
        printf("cannot open or read file <%s>\n", argv[filename_index]);
        return -1;
    }

    OpcodeVector code = scanner(file_content);

    if(interpreter_mode)
        interpreter(&code);

    if(jit_compiler_mode)
        jit(&code);

    free(file_content);
    OpcodeVector_free(&code);

    return 0;
}


