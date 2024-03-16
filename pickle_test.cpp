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
    pickle::pickle vm;
    auto st = vm.cons(vm.make_symbol("foo"), vm.make_symbol("bar"));
    for (int i = 0; i < 30; i++) {
        vm.dump(st);
        putchar('\n');
        st = vm.cons(st, vm.cons(st, NULL));
    }
    vm.gc();
    printf("all done -- cleaning up\n");
    return 0;
}