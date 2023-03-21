#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#pragma GCC optimize ("Os")
#if !defined(bool) && !defined(__cplusplus)
typedef int bool;
#define true 1
#define false 0
#endif

#define PIK_DEBUG
#define PIK_TEST

char* pik_compile(const char* code) {
    return (char*)code;
}

char* pik_eval(const char* c) {
    return (char*)c;
}

void pik_print_to(const char* c, FILE* s) {
    fprintf(s, "%s", c);
}

void pik_rep(const char* code) {
    pik_print_to(pik_eval(pik_compile(code)), stdout);
}

#ifdef PIK_TEST
void repl() {
    char* buf = (char*)malloc(64 * sizeof(char));
    size_t sz = 64;
    while (true) {
        printf("pickle> ");
        fflush(stdout);
        if (getline(&buf, &sz, stdin) == -1) {
            printf("^D\n");
            goto done;
        }
        // remove line terminator
        buf[strlen(buf) - 1] = 0;
        if (!strcmp(buf, "bye")) {
            goto done;
        }
        pik_rep(buf);
    }
    done:
    free(buf);
    return;
}

int main() {
    repl();
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
