#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>     // For dup, dup2, fileno
#include <fcntl.h>      // For open, O_RDWR

// Include all our project headers
#include "compiler.h"
#include "vm.h"
#include "jit.h"

// --- Minimal C Test Framework ---

static int g_tests_run = 0;
static int g_tests_failed = 0;

// Logs a test header. We use a macro to avoid a function call.
#define TEST_CASE(name) \
    printf("\n--- Test Case: %s ---\n", name); \
    int test_case_passed = 1

// Logs the end of a test case and updates global counters.
#define END_TEST_CASE \
    if (test_case_passed) { \
        printf("--- Result: PASSED ---\n"); \
    } else { \
        printf("--- Result: FAILED ---\n"); \
        g_tests_failed++; \
    } \
    g_tests_run++

// Main assertion macro.
#define ASSERT_TRUE(cond) \
    if (!(cond)) { \
        fprintf(stderr, "    FAILED: %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #cond); \
        test_case_passed = 0; \
    } else { \
        printf("    Passed: %s\n", #cond); \
    }

// --- Assertion Helpers ---
#define ASSERT_EQ_INT(a, b) \
    do { \
        int _a = (a); int _b = (b); \
        if (_a != _b) { \
            fprintf(stderr, "    FAILED: %s:%d: %s (%d) != %s (%d)\n", __FILE__, __LINE__, #a, _a, #b, _b); \
            test_case_passed = 0; \
        } else { \
            printf("    Passed: %s == %s (%d)\n", #a, #b, _a); \
        } \
    } while(0)

#define ASSERT_EQ_SIZE(a, b) \
    do { \
        size_t _a = (a); size_t _b = (b); \
        if (_a != _b) { \
            fprintf(stderr, "    FAILED: %s:%d: %s (%zu) != %s (%zu)\n", __FILE__, __LINE__, #a, _a, #b, _b); \
            test_case_passed = 0; \
        } else { \
            printf("    Passed: %s == %s (%zu)\n", #a, #b, _a); \
        } \
    } while(0)

#define ASSERT_EQ_STR(a, b) \
    do { \
        const char* _a = (a); const char* _b = (b); \
        if (strcmp(_a, _b) != 0) { \
            fprintf(stderr, "    FAILED: %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, _a, _b); \
            test_case_passed = 0; \
        } else { \
            printf("    Passed: \"%s\" == \"%s\"\n", _a, _b); \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    if ((ptr) == NULL) { \
        fprintf(stderr, "    FAILED: %s:%d: %s is NULL\n", __FILE__, __LINE__, #ptr); \
        test_case_passed = 0; \
    } else { \
        printf("    Passed: %s is NOT NULL\n", #ptr); \
    }

// --- End Test Framework ---


// --- Test Execution Harness ---

// A struct to hold the result of a program execution
typedef struct {
    char output[4096];
    bool success;
} RunResult;

/**
 * @brief Runs a BF program string, capturing its stdout.
 * This is the core of our behavioral testing.
 */
RunResult run_code(const char* code_string, const char* input_string, bool use_jit) {
    RunResult result = {0};
    OpcodeVector code, optimized_code;
    BfVm vm;

    // --- I/O Redirection Setup ---
    // We create temporary files to act as stdin and stdout
    FILE* tmp_in = tmpfile();
    FILE* tmp_out = tmpfile();
    if (!tmp_in || !tmp_out) {
        perror("Failed to create temp files for I/O");
        result.success = false;
        return result;
    }
    
    // Get the file descriptors
    int tmp_in_fd = fileno(tmp_in);
    int tmp_out_fd = fileno(tmp_out);
    
    // Save the real stdin/stdout
    int saved_stdin = dup(fileno(stdin));
    int saved_stdout = dup(fileno(stdout));
    
    // Write the input string to the temp input file
    fputs(input_string, tmp_in);
    rewind(tmp_in);
    
    // Redirect stdin and stdout
    dup2(tmp_in_fd, fileno(stdin));
    dup2(tmp_out_fd, fileno(stdout));
    // --- End I/O Setup ---

    // Initialize all our structures
    OpcodeVector_init(&code);
    OpcodeVector_init(&optimized_code);
    if (!BfVm_init(&vm)) {
        fprintf(stderr, "BfVm_init failed\n");
        result.success = false;
        goto cleanup;
    }

    if (!scanner(code_string, &code)) {
        fprintf(stderr, "Scanner failed\n");
        result.success = false;
        goto cleanup;
    }

    if (!optimize(&code, &optimized_code)) {
        fprintf(stderr, "Optimizer failed\n");
        result.success = false;
        goto cleanup;
    }
    
    // --- Run the code ---
    if (use_jit) {
        if (!jit_run(&vm, &optimized_code)) {
            fprintf(stderr, "JIT run failed\n");
            result.success = false;
            goto cleanup;
        }
    } else {
        interpreter(&vm, &optimized_code);
    }
    result.success = true;
    
    // --- Read the captured output ---
    fflush(stdout); // IMPORTANT: Flush stdout before reading
    rewind(tmp_out);
    size_t bytes_read = fread(result.output, 1, sizeof(result.output) - 1, tmp_out);
    result.output[bytes_read] = '\0';
    // --- End Read Output ---

cleanup:
    // --- Restore I/O ---
    fflush(stdout);
    fflush(stdin);
    dup2(saved_stdout, fileno(stdout));
    dup2(saved_stdin, fileno(stdin));
    close(saved_stdout);
    close(saved_stdin);
    fclose(tmp_in);
    fclose(tmp_out);
    // --- End I/O Restore ---
    
    OpcodeVector_free(&code);
    OpcodeVector_free(&optimized_code);
    BfVm_free(&vm);
    
    return result;
}

/**
 * @brief Main test function. Runs a program on BOTH interpreter and JIT
 * and asserts they produce the same, expected output.
 */
void test_program(const char* name, const char* code, const char* input, const char* expected_output) {
    printf("  Behavioral Test: %s\n", name);
    
    // Test Interpreter
    RunResult interp_res = run_code(code, input, false);
    ASSERT_TRUE(interp_res.success);
    ASSERT_EQ_STR(interp_res.output, expected_output);
    
    // Test JIT
    RunResult jit_res = run_code(code, input, true);
    ASSERT_TRUE(jit_res.success);
    ASSERT_EQ_STR(jit_res.output, expected_output);

    // Final sanity check: Interp vs JIT
    ASSERT_EQ_STR(interp_res.output, jit_res.output);
}


// --- Unit Test Definitions ---

void test_data_structures() {
    TEST_CASE("Data Structures");
    
    // Test IndexStack
    IndexStack stack;
    IndexStack_init(&stack);
    ASSERT_TRUE(IndexStack_is_empty(&stack));
    ASSERT_TRUE(IndexStack_push(&stack, 10));
    ASSERT_TRUE(IndexStack_push(&stack, 20));
    ASSERT_EQ_SIZE(stack.size, 2);
    
    size_t val;
    ASSERT_TRUE(IndexStack_pop(&stack, &val));
    ASSERT_EQ_SIZE(val, 20);
    ASSERT_TRUE(IndexStack_pop(&stack, &val));
    ASSERT_EQ_SIZE(val, 10);
    ASSERT_TRUE(IndexStack_is_empty(&stack));
    ASSERT_TRUE(!IndexStack_pop(&stack, &val)); // Pop from empty
    IndexStack_free(&stack);

    // Test OpcodeVector
    OpcodeVector vec;
    OpcodeVector_init(&vec);
    ASSERT_TRUE(vec.data == NULL);
    opcode op1 = {op_add, 5};
    OpcodeVector_push_back(&vec, op1);
    ASSERT_EQ_SIZE(vec.size, 1);
    ASSERT_NOT_NULL(vec.data);
    ASSERT_EQ_INT(vec.data[0].op, op_add);
    ASSERT_EQ_INT(vec.data[0].num, 5);
    OpcodeVector_free(&vec);
    
    END_TEST_CASE;
}

void test_scanner() {
    TEST_CASE("Scanner");
    
    OpcodeVector code;
    OpcodeVector_init(&code);

    // Test simple op
    ASSERT_TRUE(scanner("+++", &code));
    ASSERT_EQ_SIZE(code.size, 1);
    ASSERT_EQ_INT(code.data[0].op, op_add);
    ASSERT_EQ_INT(code.data[0].num, 3);
    OpcodeVector_free(&code);

    // Test multiple ops
    ASSERT_TRUE(scanner("+-><.,", &code));
    ASSERT_EQ_SIZE(code.size, 6);
    ASSERT_EQ_INT(code.data[0].op, op_add);
    ASSERT_EQ_INT(code.data[0].num, 1);
    ASSERT_EQ_INT(code.data[1].op, op_sub);
    ASSERT_EQ_INT(code.data[2].op, op_addp);
    ASSERT_EQ_INT(code.data[3].op, op_subp);
    ASSERT_EQ_INT(code.data[4].op, op_out);
    ASSERT_EQ_INT(code.data[5].op, op_in);
    OpcodeVector_free(&code);
    
    // Test jumps
    ASSERT_TRUE(scanner("[+]", &code));
    ASSERT_EQ_SIZE(code.size, 3);
    ASSERT_EQ_INT(code.data[0].op, op_jf);
    ASSERT_EQ_INT(code.data[0].num, 3); // Jumps past the ']'
    ASSERT_EQ_INT(code.data[1].op, op_add);
    ASSERT_EQ_INT(code.data[2].op, op_jt);
    ASSERT_EQ_INT(code.data[2].num, 0); // Jumps to the '['
    OpcodeVector_free(&code);

    // Test functions
    ASSERT_TRUE(scanner("(+!)", &code));
    ASSERT_EQ_SIZE(code.size, 4);
    ASSERT_EQ_INT(code.data[0].op, op_def_lambda);
    ASSERT_EQ_INT(code.data[0].num, 3); // Jumps past the ')'
    ASSERT_EQ_INT(code.data[1].op, op_add);
    ASSERT_EQ_INT(code.data[2].op, op_ret);
    ASSERT_EQ_INT(code.data[3].op, op_call);
    OpcodeVector_free(&code);
    
    // Test errors
    ASSERT_TRUE(!scanner("[", &code));
    ASSERT_TRUE(!scanner("]", &code));
    ASSERT_TRUE(!scanner("(", &code));
    ASSERT_TRUE(!scanner(")", &code));
    ASSERT_TRUE(!scanner("[)", &code));

    END_TEST_CASE;
}

void test_optimizer() {
    TEST_CASE("Optimizer");

    OpcodeVector code, opt_code;
    OpcodeVector_init(&code);
    
    // Test [-] optimization
    ASSERT_TRUE(scanner("[-]", &code));
    ASSERT_TRUE(optimize(&code, &opt_code));
    ASSERT_EQ_SIZE(opt_code.size, 1);
    ASSERT_EQ_INT(opt_code.data[0].op, op_clear);
    OpcodeVector_free(&code);
    OpcodeVector_free(&opt_code);
    
    // Test [+] optimization
    ASSERT_TRUE(scanner("[+]", &code));
    ASSERT_TRUE(optimize(&code, &opt_code));
    ASSERT_EQ_SIZE(opt_code.size, 1);
    ASSERT_EQ_INT(opt_code.data[0].op, op_clear);
    OpcodeVector_free(&code);
    OpcodeVector_free(&opt_code);

    // Test non-optimization
    ASSERT_TRUE(scanner("[->+<]", &code));
    ASSERT_TRUE(optimize(&code, &opt_code));
    ASSERT_EQ_SIZE(opt_code.size, 6); // Should not optimize
    ASSERT_EQ_INT(opt_code.data[0].op, op_jf);
    OpcodeVector_free(&code);
    OpcodeVector_free(&opt_code);
    
    END_TEST_CASE;
}

void test_execution() {
    TEST_CASE("Program Execution (Interpreter vs JIT)");

    test_program("Hello", 
        "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>." // H
        "---." // e
        "++++++ +." // l
        "." // l
        "+++." // o
        , "", "Hello");
    
    test_program("Echo", 
        ",.", 
        "A", "A");
    
    test_program("Cat",
        ",[.,]",
        "Hi!\n", "Hi!\n");
        
    test_program("Loop",
        "+++[>+.<-]",
        "", " \x01\x02"); // Prints ASCII 1, 2, 3

    test_program("Hardcoded Multiply",
        "++++++(>+++++++<) > [-<+>]! .", // 6 * 7
        "",
        "*"); // ASCII 42

    test_program("Lambda Call",
        "(+).", // Define 'inc', print (0)
        "",
        "\x00"); // Should just print 0

    test_program("Lambda Call 2",
        "(+)!.", // Define 'inc', call, print (1)
        "",
        "\x01");

    test_program("Lambda Capture",
        "+++++ > + (>.<) ! .", // p[0]=5, p[1]=1. Def lambda (capture p[1]). Call. Print p[1].
        "",
        "\x01"); // Call should not affect p[1]

    test_program("Lambda Capture 2",
        "+++++ > + ( < + > ) ! . > .", // p[0]=5, p[1]=1. Def lambda (capture p[1]). Call. Print p[1]. Go to p[2]. Print p[2].
        "",                            // Lambda body is <+> (inc p[0])
        "\x01\x00");                   // Should print p[1] (1) and p[2] (0). p[0] becomes 6.
    
    END_TEST_CASE;
}


// --- Main Test Runner ---
int main(void) {
    printf("===== Running Brainfork Test Suite =====\n");
    
    test_data_structures();
    test_scanner();
    test_optimizer();
    test_execution();
    
    if (g_tests_failed > 0) {
        printf("\n======= %d / %d TESTS FAILED =======\n", g_tests_failed, g_tests_run);
        return 1;
    }
    
    printf("\n======= ALL %d TESTS PASSED =======\n", g_tests_run);
    return 0;
}

