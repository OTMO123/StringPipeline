#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int run_test(const char* name, const char* path) {
    printf("\n========================================\n");
    printf("Running: %s\n", name);
    printf("========================================\n");
    
    int pid = fork();
    if (pid == 0) {
        execl(path, path, NULL);
        perror("execl failed");
        exit(1);
    }
    
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("✅ %s PASSED\n", name);
        return 0;
    } else {
        printf("❌ %s FAILED\n", name);
        return 1;
    }
}

int main(void) {
    int failures = 0;
    
    failures += run_test("Queue Tests", "./build/bin/test_queue");
    failures += run_test("Monitor Tests", "./build/bin/test_monitor");
    
    printf("\n========================================\n");
    printf("TEST SUMMARY\n");
    printf("========================================\n");
    if (failures == 0) {
        printf("✅ ALL TESTS PASSED!\n");
    } else {
        printf("❌ %d TEST SUITE(S) FAILED!\n", failures);
    }
    
    return failures > 0 ? 1 : 0;
}
