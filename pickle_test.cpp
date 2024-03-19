#define TINOBSY_DEBUG
#include "pickle.hpp"

#include <cstdio>
#include <csignal>

using pvm = pickle::pickle;
using pickle::object;

object* test_test(pvm* vm, object* data) {
    printf("Hello from %s()!\n", __func__);
    vm->dump(data);
    putchar('\n');
    object* d = vm->pop();
    vm->dump(d);
    putchar('\n');
    if (d) vm->push_inst("test_test", "debug", vm->string("called from inside test_test()"));
    return vm->sym(d ? "debug" : "error");
}

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
    pvm vm;
    vm.defop("test_test", test_test);
    auto last_pair = vm.cons(vm.integer(4), nil);
    auto st = vm.cons(vm.integer(1), vm.cons(vm.integer(2), vm.cons(vm.integer(3), last_pair)));
    cdr(last_pair) = st;
    printf("st data: ");
    vm.dump(st);
    vm.start_thread();
    vm.start_thread();
    vm.push_inst("test_test", "error", vm.string("from error handler"));
    vm.push_inst("test_test", nil, vm.string("normal"));
    vm.push_data(vm.integer(42));
    vm.push_data(vm.integer(42));
    vm.push_data(vm.integer(42));
    vm.push_data(vm.integer(42));
    printf("\nqueue with data: ");
    vm.dump(vm.queue);
    putchar('\n');
    while (vm.queue) {
        vm.step();
        vm.gc();
        printf("queue -> ");
        vm.dump(vm.queue);
        putchar('\n');
    }
    printf("all done -- cleaning up\n");
    // implicit destruction of vm;
    return 0;
}
