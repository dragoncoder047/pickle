#define TINOBSY_DEBUG
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


lambda x
    foo 123
    bar 456






123foo123]

)=";

#define SEPARATOR printf("\n\n----------------------------------------------------------------------------------------\n\n")

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
    vm.push_inst("parse");
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
    SEPARATOR;
    printf("hashmap test\n");
    auto foo = vm.newobject();
    for (size_t i = 0; i < 10; i++) {
        vm.set_property(foo, vm.integer(i), i, vm.integer(i));
        printf("Dump of object: ");
        vm.dump(foo);
        putchar('\n');
    }
    for (size_t i = 0; i < 10; i++) {
        vm.set_property(foo, vm.integer(i), i, vm.integer(i));
        printf("Dump of object: ");
        vm.dump(foo);
        putchar('\n');
    }
    ASSERT(vm.get_property(foo, 0) != nil);
    auto pair = vm.get_property(foo, 0);
    ASSERT(car(foo) == pair);
    putchar('\n');
    vm.dump(foo);
    SEPARATOR;
    printf("all done -- cleaning up\n");
    // implicit destruction of vm;
    return 0;
}
