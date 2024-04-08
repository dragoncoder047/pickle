#ifndef PICKLE_H
#define PICKLE_H

#include "tinobsy/tinobsy.hpp"
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

namespace pickle {

using tinobsy::object;
using tinobsy::object_type;

// used for places where NULL would be ambiguous
#define nil ((object*)NULL)

class pvm;

typedef object* (*func_ptr)(pvm* vm, object* cookie, object* inst_type);

extern const object_type cons_type;
extern const object_type obj_type;
extern const object_type c_function_type;
extern const object_type string_type;
extern const object_type symbol_type;
extern const object_type integer_type;
extern const object_type float_type;

class pvm : public tinobsy::vm {
    public:
    pvm();

    // round-robin queue of threads (circular list)
    object* queue = NULL;

    // global scope
    object* globals = NULL;

    // alist of all of the registered instructions
    object* function_registry = NULL;

    // pushes the thing onto the cons stack: stack = cons(thing, stack)
    inline void push(object* thing, object*& stack) {
        stack = this->cons(thing, stack);
    }
    // pops the top data from the queue - returns nil if the stack is empty
    inline object* pop(object*& stack) {
        if (!stack) return nil;
        object* data = car(stack);
        stack = cdr(stack);
        return data;
    }

    // pushes the data to the current thread's data stack
    inline void push_data(object* thing) {
        object* ct = this->curr_thread();
        if (!ct) return;
        this->push(thing, car(ct));
    }

    // pushes the data to the current thread's instruction stack
    // inst is the symbol name (to look up in function_registry), type is the kind of the function, cookie is optional
    inline void push_inst(const char* inst, object* type = nil, object* cookie = nil) {
        this->push_inst(this->sym(inst), type, cookie);
    }
    inline void push_inst(const char* inst, const char* type, object* cookie = nil) {
        this->push_inst(this->sym(inst), type, cookie);
    }
    inline void push_inst(object* inst, const char* type, object* cookie = nil) {
        this->push_inst(inst, this->sym(type), cookie);
    }
    inline void push_inst(object* inst, object* type = nil, object* cookie = nil) {
        object* ct = this->curr_thread();
        if (!ct) return;
        this->push(this->cons(type, this->cons(inst, cookie)), cdr(cdr(ct)));
    }

    // pops data from the current thread's data stack
    inline object* pop() {
        object* curr_thread = this->curr_thread();
        if (!curr_thread) return nil;
        return this->pop(car(curr_thread));
    }

    // adds a function to the function registry
    inline void defop(const char* name, func_ptr fptr) {
        this->push(this->cons(this->sym(name), this->func(fptr)), this->function_registry);
    }

    // unbox a function
    inline object* func(func_ptr f) {
        INTERN(this, func_ptr, &c_function_type, f);
        object* o = this->alloc(&c_function_type);
        o->as_ptr = (void*)f;
        return o;
    }

    // box a function
    inline func_ptr fptr(object* f) {
        ASSERT(f != nil && f->type == &c_function_type);
        return (func_ptr)f->as_ptr;
    }

    // box a C string
    inline object* string(const char* chs) {
        ASSERT(chs != NULL);
        INTERN_PTR(this, const char*, &string_type, chs, [](const char* a, const char* b) -> bool { return !strcmp(a, b); });
        object* o = this->alloc(&string_type);
        o->as_chars = strdup(chs);
        return o;
    }

    // unbox a C string or a symbol
    inline const char* const stringof(object* s) {
        ASSERT(s != nil && (s->type == &string_type || s->type == &symbol_type));
        return s->as_chars;
    }

    // create a symbol
    inline object* sym(const char* symbol) {
        ASSERT(symbol != NULL);
        INTERN_PTR(this, const char*, &symbol_type, symbol, [](const char* a, const char* b) -> bool { return !strcmp(a, b); });
        object* o = this->alloc(&symbol_type);
        o->as_chars = strdup(symbol);
        return o;
    }

    // create a cons cell
    inline object* cons(object* xar, object* xdr) {
        object* o = this->alloc(&cons_type);
        car(o) = xar;
        cdr(o) = xdr;
        return o;
    }

    // box an integer
    inline object* integer(int64_t x) {
        INTERN(this, int64_t, &integer_type, x);
        object* o = this->alloc(&integer_type);
        o->as_big_int = x;
        return o;
    }

    // unbox an integer
    inline int64_t intof(object* x) {
        ASSERT(x != nil && x->type == &integer_type);
        return x->as_big_int;
    }

    // box a floating point number
    inline object* number(double x) {
        INTERN(this, int64_t, &float_type, x);
        object* o = this->alloc(&float_type);
        o->as_double = x;
        return o;
    }

    // unbox a floating point number
    inline double numof(object* x) {
        ASSERT(x != nil && x->type == &float_type);
        return x->as_double;
    }

    // Returns a new empty object, of the specified prototypes (which must be a cons list)
    inline object* newobject(object* prototypes = nil) {
        object* o = this->alloc(&obj_type);
        car(o) = prototypes;
        cdr(o) = nil;
        return o;
    }

    // Looks up the property on the object, optionally recursing into prototypes if it's not found directly.
    // If it is not found anywhere return nil.
    object* get_property(object* obj, uint64_t hash, bool recurse = false);

    // Sets the property directly on the object. Returns true if setting succeeded.
    bool set_property(object* obj, object* key, uint64_t hash, object* value);

    // Sets the property directly on the object. Returns true if something was removed.
    bool remove_property(object* obj, uint64_t hash);

    // execute one instruction on the current thread and go to the next thread
    void step();

    // push a new empty thread to the thread queue
    void start_thread();

    // write the object to stdout using srfi 38 write/ss alike formatting
    void dump(object*);



    // overridden garbage collect
    size_t gc();


    private:
    // marks reachable objects
    void mark_globals();

    // get the current thread - nil if there are no threads
    inline object* curr_thread() {
        if (!this->queue) return nil;
        return car(this->queue);
    }

    // gets the next instruction from the current thread or nil if no thread
    inline object* pop_inst() {
        object* curr_thread = this->curr_thread();
        if (!curr_thread) return nil;
        return this->pop(cdr(cdr(curr_thread)));
    }

    int hash_seed;
};


// Helper functions.

// Returns 0 if equal, or nonzero if not equal. Doesn't work on compound or user types
int eqcmp(object*, object*);
// Returns the pair in the assoc list that has the same key, or NULL if not found.
object* assoc(object*, object*);
// Removes the key/value pair from the list and returns it, or returns NULL if the pair never existed.
object* delassoc(object**, object*);

namespace parser {
object* tokenize(pvm* vm, object* cookie, object* inst_type);
}

object* eval(pvm* vm, object* cookie, object* inst_type);
object* splice_match(pvm* vm, object* cookie, object* inst_type);
}

#include "pickle.cpp"

#endif
