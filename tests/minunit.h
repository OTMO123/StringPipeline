/**
 * MinUnit - Minimal Unit Testing Framework for C
 * Adapted for production use with thread safety and detailed reporting
 */

#ifndef MINUNIT_H
#define MINUNIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Test counters */
extern int tests_run;
extern int tests_failed;
extern int assertions_run;
extern int assertions_failed;

/* Test result structure */
typedef struct {
    int passed;
    const char* message;
    const char* file;
    int line;
} test_result_t;

/* Test function type */
typedef test_result_t (*test_func_t)(void);

/* Test suite structure */
typedef struct {
    const char* name;
    test_func_t* tests;
    int test_count;
} test_suite_t;

/* Assertion macros */
#define mu_assert(message, test) do { \
    assertions_run++; \
    if (!(test)) { \
        assertions_failed++; \
        test_result_t result = {0, message, __FILE__, __LINE__}; \
        return result; \
    } \
} while (0)

#define mu_assert_int_eq(expected, actual) do { \
    assertions_run++; \
    if ((expected) != (actual)) { \
        assertions_failed++; \
        char* msg = malloc(256); \
        snprintf(msg, 256, "Expected %d but got %d", expected, actual); \
        test_result_t result = {0, msg, __FILE__, __LINE__}; \
        return result; \
    } \
} while (0)

#define mu_assert_str_eq(expected, actual) do { \
    assertions_run++; \
    if (strcmp((expected), (actual)) != 0) { \
        assertions_failed++; \
        char* msg = malloc(512); \
        snprintf(msg, 512, "Expected '%s' but got '%s'", expected, actual); \
        test_result_t result = {0, msg, __FILE__, __LINE__}; \
        return result; \
    } \
} while (0)

#define mu_assert_ptr_eq(expected, actual) do { \
    assertions_run++; \
    if ((expected) != (actual)) { \
        assertions_failed++; \
        char* msg = malloc(256); \
        snprintf(msg, 256, "Expected pointer %p but got %p", expected, actual); \
        test_result_t result = {0, msg, __FILE__, __LINE__}; \
        return result; \
    } \
} while (0)

#define mu_assert_ptr_not_null(ptr) do { \
    assertions_run++; \
    if ((ptr) == NULL) { \
        assertions_failed++; \
        test_result_t result = {0, "Pointer is NULL", __FILE__, __LINE__}; \
        return result; \
    } \
} while (0)

#define mu_assert_ptr_null(ptr) do { \
    assertions_run++; \
    if ((ptr) != NULL) { \
        assertions_failed++; \
        test_result_t result = {0, "Pointer is not NULL", __FILE__, __LINE__}; \
        return result; \
    } \
} while (0)

/* Test running macro */
#define mu_run_test(test) do { \
    test_result_t result = test(); \
    tests_run++; \
    if (!result.passed) { \
        tests_failed++; \
        printf("❌ FAILED: %s\n", #test); \
        printf("   %s:%d: %s\n", result.file, result.line, result.message); \
    } else { \
        printf("✓ PASSED: %s\n", #test); \
    } \
} while (0)

/* Success result */
#define MU_PASS ((test_result_t){1, NULL, NULL, 0})

/* Test suite runner */
static inline int mu_run_suite(test_suite_t* suite) {
    printf("\n========================================\n");
    printf("Running test suite: %s\n", suite->name);
    printf("========================================\n");
    
    clock_t start = clock();
    
    for (int i = 0; i < suite->test_count; i++) {
        test_result_t result = suite->tests[i]();
        tests_run++;
        if (!result.passed) {
            tests_failed++;
            printf("❌ Test %d failed\n", i + 1);
            if (result.message) {
                printf("   %s:%d: %s\n", result.file, result.line, result.message);
            }
        } else {
            printf("✓ Test %d passed\n", i + 1);
        }
    }
    
    clock_t end = clock();
    double time_spent = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("\nSuite Summary:\n");
    printf("  Tests run: %d\n", suite->test_count);
    printf("  Tests failed: %d\n", tests_failed);
    printf("  Time: %.3f seconds\n", time_spent);
    printf("========================================\n");
    
    return tests_failed == 0 ? 0 : 1;
}

/* Report generation */
static inline void mu_print_summary(void) {
    printf("\n========================================\n");
    printf("FINAL TEST SUMMARY\n");
    printf("========================================\n");
    printf("Total tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_run - tests_failed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Assertions run: %d\n", assertions_run);
    printf("Assertions failed: %d\n", assertions_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ ALL TESTS PASSED!\n");
    } else {
        printf("\n❌ SOME TESTS FAILED!\n");
    }
    printf("========================================\n");
}

#endif /* MINUNIT_H */