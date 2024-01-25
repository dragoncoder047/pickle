#define TINOBSY_DEBUG
#include "pickle.hpp"

#include <cstdio>
#include <csignal>

void on_segfault(int signal, siginfo_t* info, void* arg) {
    fprintf(stderr, "Segmentation fault at %p\n", info->si_addr);
    DBG("Segmentation fault at %p", info->si_addr);
    exit(255);
}

void start_catch_segfault() {
    struct sigaction x = { 0 };
    sigemptyset(&x.sa_mask);
    x.sa_sigaction = on_segfault;
    x.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &x, NULL);
}

int main() {
    start_catch_segfault();
    auto vm = new pickle::pickle();
    auto foo = vm->with_metadata(vm->wrap_string("foo\n    bar\n  syntax error"), 1, 1, "foo.pickle", vm->list(3, NULL, NULL, NULL));
    pickle::funcs::parse(vm, vm->list(1, foo), NULL, vm->wrap_func(PICKLE_INLINE_FUNC {
            printf("Foofunc called. Args[0] should be a string, value = %s\n", car(args)->cells[0].as_chars);
    }), NULL);
    while (vm->queue_head) vm->run_next_thunk(), vm->gc();
    printf("all done\n");
    delete vm;
    return 0;
}