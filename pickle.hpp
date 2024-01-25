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
using tinobsy::object_schema;

char escape(char c);
char unescape(char c);
bool needs_escape(char c);

class pickle;

typedef void (*func_ptr)(pickle* runner, object* args, object* env, object* cont, object* fail_cont);

extern const object_schema metadata_type;
extern const object_schema cons_type;
extern const object_schema partial_type;
extern const object_schema c_function_type;
extern const object_schema string_type;
extern const object_schema symbol_type;
extern const object_schema stream_type;
extern const object_schema error_type;
extern const object_schema integer_type;
extern const object_schema float_type;
extern const object_schema list_type;

class pickle : public tinobsy::vm {
    public:
    object* queue_head = NULL;
    object* queue_tail = NULL;
    object* globals = NULL;
    object* list(size_t len, ...);
    object* append(object* l1, object* l2);
    void set_retval(object* args, object* env, object* cont, object* fail_cont);
    void set_failure(object* err, object* env, object* cont, object* fail_cont);
    void do_later(object* thunk);
    void do_next(object* thunk);
    void run_next_thunk();
    inline object* wrap_func(func_ptr f) {
        return this->allocate(&c_function_type, f);
    }
    inline func_ptr unwrap_func(object* f) {
        ASSERT(f != NULL && f->schema == &c_function_type);
        return (func_ptr)f->as_ptr;
    }
    inline object* make_partial(object* func, object* args, object* env, object* continuation, object* failure_continuation) {
        return this->allocate(&partial_type, func, args, env, continuation, failure_continuation);
    }
    inline object* wrap_string(const char* chs) {
        return this->allocate(&string_type, chs);
    }
    inline const char* const unwrap_string(object* s) {
        ASSERT(s != NULL && s->schema == &string_type);
        return s->cells[0].as_chars;
    }
    inline object* wrap_symbol(const char* symbol) {
        return this->allocate(&symbol_type, symbol);
    }
    inline object* cons(object* car, object* cdr) {
        return this->allocate(&cons_type, car, cdr);
    }
    inline object* wrap_error(object* type, object* details, object* continuation) {
        return this->allocate(&error_type, type, details, continuation);
    }
    inline object* wrap_integer(int64_t x) {
        return this->allocate(&integer_type, x);
    }
    inline object* wrap_float(double x) {
        return this->allocate(&float_type, x);
    }
    inline object* wrap_metadata(object* line, object* col, object* file, object* prototypes) {
        return this->allocate(&metadata_type, line, col, file, prototypes);
    }
    inline object* wrap_metadata(int64_t line, int64_t col, const char* file, object* prototypes) {
        return this->allocate(&metadata_type, this->wrap_integer(line), this->wrap_integer(col), this->wrap_string(file), prototypes);
    }
    inline object* with_metadata(object* x, int64_t line, int64_t col, const char* file, object* prototypes) {
        if (x->meta) {
            x->meta->cells[0].as_obj = this->wrap_integer(line);
            x->meta->cells[1].as_obj = this->wrap_integer(col);
            x->meta->cells[2].as_obj = this->wrap_string(file);
            x->meta->cells[2].as_obj = prototypes;
        } else {
            x->meta = this->wrap_metadata(line, col, file, prototypes);
        }
        return x;
    }
    private:
    void mark_globals();
};


namespace funcs {
    void parse(pickle* vm, object* args, object* env, object* cont, object* fail_cont);
    void eval(pickle* vm, object* args, object* env, object* cont, object* fail_cont);
    void splice_match(pickle* vm, object* args, object* env, object* cont, object* fail_cont);
}

void getarg(pickle* vm, object* args, size_t nth, const object_schema* type, object* env, object* fail, object* then);

}

#define car(x) ((x)->cells[0].as_obj)
#define cdr(x) ((x)->cells[1].as_obj)
#define PICKLE_INLINE_FUNC [](::pickle::pickle* vm, ::pickle::object* args, ::pickle::object* env, ::pickle::object* cont, ::pickle::object* fail_cont) -> void
#define GOTTEN_ARG(nm) auto nm = car(args); args = car(cdr(args))

#ifdef TINOBSY_DEBUG
#define TODO(nm) do { DBG("%s: %s", #nm, strerror(ENOSYS)); errno = ENOSYS; perror(#nm); exit(74); } while (0)
#else
#define TODO(nm)
#endif

#include "pickle.cpp"

#endif
