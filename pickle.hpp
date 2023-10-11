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

// A struct to hold the line/column information for tokens.
class location {
    public:
    location();
    location(const location* other);
    location(size_t line, size_t col, const char* name);
    ~location();
    size_t line = 1;
    size_t col = 1;
    char* name = NULL;
};

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

class pickle : public tinobsy::vm {
    public:
    object* queue_head = NULL;
    object* queue_tail = NULL;
    object* cons_list(size_t len, ...);
    object* append(object* l1, object* l2);
    void set_retval(object* args, object* env, object* cont, object* fail_cont);
    void set_failure(object* err, object* env, object* cont, object* fail_cont);
    void do_later(object* thunk);
    void do_next(object* thunk);
    void run_next_thunk();
    inline object* wrap_func(func_ptr f) {
        return this->allocate(&c_function_type, f);
    }
    inline object* make_partial(object* func, object* args, object* env, object* continuation, object* failure_continuation) {
        return this->allocate(&partial_type, func, args, env, continuation, failure_continuation);
    }
    object* wrap_string(const char* chs);
    inline object* wrap_symbol(const char* symbol) {
        return this->allocate(&symbol_type, symbol);
    }
    inline object* cons(object* car, object* cdr) {
        return this->allocate(&cons_type, car, cdr);
    }
    inline object* make_error(object* type, object* details, object* continuation) {
        return this->allocate(&error_type, type, details, continuation);
    }
    private:
    void mark_globals();
};


namespace funcs {
    void parse(pickle* runner, object* args, object* env, object* cont, object* fail_cont);
    void eval(pickle* runner, object* args, object* env, object* cont, object* fail_cont);
    void splice_match(pickle* runner, object* args, object* env, object* cont, object* fail_cont);
}

}

#define car(x) ((x)->cells[0].as_obj)
#define cdr(x) ((x)->cells[1].as_obj)

#ifdef TINOBSY_DEBUG
#define TODO do { DBG("%s: %s", __func__, strerror(ENOSYS)); errno = ENOSYS; perror(__func__); exit(74); } while (0)
#else
#define TODO
#endif

#include "pickle.cpp"

#endif
