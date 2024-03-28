//#define TINOBSY_DEBUG
#include "pickle.hpp"

#include <stdio.h>

using pickle::pvm;
using pickle::object;

object* test_test(pvm* vm, object* cookie, object* inst_type) {
    printf("Hello from test_test()!\ninst_type = ");
    vm->dump(inst_type);
    printf("\ncookie = ");
    vm->dump(cookie);
    printf("\ntop of stack = ");
    object* d = vm->pop();
    vm->dump(d);
    putchar('\n');
    if (d) vm->push_inst("test_test", "debug", vm->string("from inside test_test()"));
    return vm->sym(d ? "debug" : "error");
}

const char* test = R"=(

[(+ 1 2)
## #### block comment '











foo123]

)=";

int main() {
    pvm vm;
    vm.defop("parse", pickle::parse);
    vm.defop("test_test", test_test);
    auto last_pair = vm.cons(vm.integer(2), nil);
    auto st = vm.cons(vm.integer(1), vm.cons(vm.integer(2), vm.cons(vm.integer(1), last_pair)));
    cdr(last_pair) = st;
    printf("st data: ");
    vm.dump(st);
    vm.start_thread();
    vm.start_thread();
    vm.push_inst("test_test", "error", vm.string("from error handler"));
    vm.push_inst("test_test", nil, vm.string("output result"));
    vm.push_inst("parse", nil, vm.string("normal"));
    vm.push_data(vm.integer(42));
    vm.push_data(st);
    vm.push_data(vm.integer(42));
    vm.push_data(vm.string(test));
    printf("\nqueue with data: ");
    vm.dump(vm.queue);
    putchar('\n');
    while (vm.queue) {
        vm.step();
        vm.gc();
        printf("\nqueue = ");
        vm.dump(vm.queue);
        printf("\n\n");
    }
    printf("all done -- cleaning up\n");
    // implicit destruction of vm;
    return 0;
}
