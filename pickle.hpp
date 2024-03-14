#ifndef PICKLE_H
#define PICKLE_H

#include "tinobsy/tinobsy.hpp"
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdarg>

namespace pickle {

using tinobsy::object;
using tinobsy::object_type;

char escape(char c);
char unescape(char c);
bool needs_escape(char c);

class pickle;

typedef object* (*func_ptr)(pickle* vm);

extern const object_type metadata_type;
extern const object_type cons_type;
extern const object_type partial_type;
extern const object_type c_function_type;
extern const object_type string_type;
extern const object_type symbol_type;
extern const object_type stream_type;
extern const object_type error_type;
extern const object_type integer_type;
extern const object_type float_type;
extern const object_type list_type;

class pickle : public tinobsy::vm {
    public:
    object* queue_head = NULL;
    object* queue_tail = NULL;
    object* globals = NULL;
    object* stack = NULL;
    object* instruction_stack = NULL;
    object* function_registry = NULL;
    inline void push(object* thing, object*& stack) {
        stack = this->cons(thing, stack);
    }
    inline void push_data(object* thing) {
        this->push(thing, this->stack);
    }
    inline void push_instruction(object* inst, object* type = NULL) {
        this->push(this->cons(type, inst), this->instruction_stack);
    }
    inline void register_function(const char* name, func_ptr fptr) {
        this->push(this->cons(this->make_symbol(name), this->make_func(fptr)), this->function_registry);
    }
    inline object* make_func(func_ptr f) {
        INTERN(this, func_ptr, &c_function_type, f);
        object* o = this->alloc(&c_function_type);
        o->as_ptr = (void*)f;
        return o;
    }
    inline func_ptr unwrap_func(object* f) {
        ASSERT(f != NULL && f->type == &c_function_type);
        return (func_ptr)f->as_ptr;
    }
    inline object* make_string(const char* chs) {
        INTERN_PTR(this, const char*, &string_type, chs, [](const char* a, const char* b) -> bool { return !strcmp(a, b); });
        object* o = this->alloc(&string_type);
        o->as_chars = strdup(chs);
        return o;
    }
    inline const char* const unwrap_string(object* s) {
        ASSERT(s != NULL && s->type == &string_type);
        return s->as_chars;
    }
    inline object* make_symbol(const char* symbol) {
        INTERN_PTR(this, const char*, &symbol_type, symbol, [](const char* a, const char* b) -> bool { return !strcmp(a, b); });
        object* o = this->alloc(&symbol_type);
        o->as_chars = strdup(symbol);
        return o;
    }
    inline object* cons(object* car, object* cdr) {
        object* o = this->alloc(&cons_type);
        car(o) = car;
        cdr(o) = cdr;
        return o;
    }
    inline object* make_integer(int64_t x) {
        INTERN(this, int64_t, &integer_type, x);
        object* o = this->alloc(&integer_type);
        o->as_big_int = x;
        return o;
    }
    inline object* make_float(double x) {
        INTERN(this, int64_t, &float_type, x);
        object* o = this->alloc(&float_type);
        o->as_double = x;
        return o;
    }


    void step();


    private:
    void mark_globals();
};


// Helper functions.

// Returns 0 if equal, or nonzero if not equal. Doesn't work on compound or user types
int prim_cmp(object*, object*);
// Returns the pair in the assoc list that has the same key, or NULL if not found.
object* assoc(object*, object*);
// Removes the key/value pair from the list and returns it, or returns NULL if the pair never existed.
object* delassoc(object**, object*);

void parse(pickle* vm, object* args, object* env, object* cont, object* fail_cont);
void eval(pickle* vm, object* args, object* env, object* cont, object* fail_cont);
void splice_match(pickle* vm, object* args, object* env, object* cont, object* fail_cont);

// Chokes on self-referential objects -- you have been warned
void dump(object*);


}

#include "pickle.cpp"

#endif
