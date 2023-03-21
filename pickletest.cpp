#include <stdio.h>
#include <stdlib.h>
#define PICKLE_TEST
#define PICKLE_DEBUG
#include "pickle.h"

void repl(PickleVM vm) {
    char* buf = new char[64];
    char* codebuf = new char[64];
    size_t sz = 64;
    for (;;) {
        printf("pickle> ");
        fflush(stdout);
        codebuf[0] = 0;
        for (;;) {
            if (getline(&buf, &sz, stdin) == -1) {
                printf("^D\n");
                goto done;
            }
            if (strlen(buf) == 1) break; // getline includes the terminating newline
            printf("   ...> ");
            fflush(stdout);
            char* newcode;
            asprintf(&newcode, "%s%s", codebuf, buf);
            delete[] codebuf;
            codebuf = newcode;
        }
        if (!strncmp(codebuf, "bye", 3)) {
            goto done;
        }
        // if (!strncmp(codebuf, "dir", 3)) {
        //     pik_print_to(vm, vm->global_scope, stdout);
        //     putchar('\n');
        //     continue;
        // }
        // pik_mark_garbage(vm);
        // if (pik_compile(vm, codebuf, vm->global_scope) == RERROR) {
        //     printf("Compile error!\n%s\n", vm->global_scope->result->message);
        //     continue;
        // }
        // printf("executing:\n");
        // pik_print_to(vm, vm->global_scope->result, stdout);
        // putchar('\n');
        // if (pik_eval(vm, NULL, vm->global_scope->result, NULL, vm->global_scope) == RERROR) {
        //     printf("Execution error!\n%s\n", vm->global_scope->result->message);
        //     continue;
        // }
        // printf("result> ");
        // pik_print_to(vm, vm->global_scope->result, stdout);
        // putchar('\n');
        printf("you entered: %s\n", codebuf);
    }
    done:
    delete[] buf;
    delete[] codebuf;
}

int main() {
    PickleVM vm;
    repl(vm);
    return 0;
}