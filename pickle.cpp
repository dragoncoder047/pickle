#include "pickle.hpp"
#include <errno.h>
#include <inttypes.h>

namespace pickle {

char unescape(char c) {
    switch (c) {
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
        case 'a': return '\a';
        case 'o': return '{';
        case 'c': return '}';
        case '\n': return 0;
        default: return c;
    }
}

char escape(char c) {
    switch (c) {
        case '\b': return 'b';
        case '\t': return 't';
        case '\n': return 'n';
        case '\v': return 'v';
        case '\f': return 'f';
        case '\r': return 'r';
        case '\a': return 'a';
        case '{': return 'o';
        case '}': return 'c';
        default: return c;
    }
}

bool needs_escape(char c) {
    return strchr("{}\b\t\n\v\f\r\a\\\"", c) != NULL;
}

void free_payload(object* o) { free(o->as_ptr); }

// ------------------------ core types -----------------
// these will later be swapped for actual objects

// cons = car, cdr
const object_type cons_type("cons", tinobsy::markcons, NULL, NULL);
// --------- primitive/ish types ---------------
const object_type string_type("string", NULL, free_payload, NULL);
const object_type symbol_type("symbol", NULL, free_payload, NULL);
const object_type c_function_type("c_function", NULL, NULL, NULL);
const object_type integer_type("int", NULL, NULL, NULL);
const object_type float_type("float", NULL, NULL, NULL);
const object_type* primitives[] = { &string_type, &symbol_type, &c_function_type, &integer_type, &float_type, NULL };

void pickle::mark_globals() {
    this->markobject(this->queue_head);
    this->markobject(this->queue_tail);  // in case queue gets detached
    this->markobject(this->globals);
    this->markobject(this->stack);
}

void pickle::step() {
    DBG("TODO: write step stack code");
}

//--------------- HELPER FUNCTIONS ----------------------------

static bool is_primitive_type(object* x) {
    if (x == NULL) return true;
    size_t i = 0; while (primitives[i] != NULL) { if (x->type == primitives[i]) break; }
    return primitives[i] != NULL;
}

int prim_cmp(object* a, object* b) {
    if (a == b) return 0;
    if (a == NULL) return -1;
    if (b == NULL) return 1;
    if (a->type != b->type) return a->type - b->type;
    if (!is_primitive_type(a)) return -1;
    if (a->type->free) return strcmp(a->as_chars, b->as_chars); // String-ish type
    if (a->type == &float_type) return a->as_double - b->as_double;
    return a->as_big_int - b->as_big_int;
}

object* assoc(object* list, object* key) {
    for (; list; list = cdr(list)) {
        object* pair = car(list);
        if (!prim_cmp(key, car(pair))) return pair;
    }
    return NULL;
}

object* delassoc(object** list, object* key) {
    for (; *list; list = &cdr(*list)) {
        object* pair = car(*list);
        if (!prim_cmp(key, car(pair))) {
            *list = cdr(*list);
            return pair;
        }
    }
    return NULL;
}

//--------------- PARSER --------------------------------------

// Can be called by the program
void parse(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    DBG("parsing");
    // getarg(vm, args, 0, &string_type, env, fail_cont, vm->wrap_func(PICKLE_INLINE_FUNC {
    //     GOTTEN_ARG(s);
    //     const char* str = (const char*)(s->cells[0].as_chars);
    //     object* result = s->cells[1].as_obj;
    //     const char* message;
    //     bool success = true;
    //     if (result) { // Saved preparse
    //         if (result->schema == &error_type) success = false;
    //         goto done;
    //     }
    //     result = vm->wrap_string("Hello, World! parse result i am."); /* TODO: replace this with the actual parse code */
    //     done:
    //     if (success) vm->set_retval(vm->list(1, result), env, cont, fail_cont);
    //     else {
    //         result = vm->wrap_error(vm->wrap_symbol("SyntaxError"), vm->list(1, vm->wrap_string(message), result), cont);
    //         vm->set_failure(result, env, cont, fail_cont);
    //     }
    //     s->cells[1].as_obj = result; // Save parse for later if constantly eval'ing string (i.e. a loop)
    // }));
}

static object* get_best_match(pickle* vm, object* ast, object** env) {
    return NULL;
}

// Eval(list) ::= apply_first_pattern(list), then eval(remaining list), else list if no patterns match
void eval(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    // object* ast = car(args);
    // // returns Match object: 0=pattern, 1=handler body, 2=match details for splice; and updates env with bindings
    // object* oldenv = env;
    // object* matched_pattern = get_best_match(vm, ast, &env);
    // if (matched_pattern != NULL) {
    //     // do next is run body --> cont=apply match cont-> eval again -> original eval cont
    //     vm->do_later(vm->make_partial(
    //         NULL,//matched_pattern->body(),
    //         NULL,
    //         env,
    //         vm->make_partial(
    //             vm->wrap_func(funcs::splice_match),
    //             vm->list(2, vm->append(ast, NULL), NULL/*matched_pattern->match_info()*/),
    //             oldenv,
    //             vm->make_partial(
    //                 vm->wrap_func(funcs::eval),
    //                 NULL,
    //                 oldenv,
    //                 cont,
    //                 fail_cont
    //             ),
    //             fail_cont
    //         ),
    //         fail_cont
    //     ));
    // } else {
    //     // No matches so return unchanged
    //     vm->set_retval(vm->list(1, ast), env, cont, fail_cont);
    // }
}

void splice_match(pickle* vm, object* args, object* env, object* cont, object* fail_cont) {
    // TODO(sm);
}

static void count_pointers() {

}

void dump(object* x) {
    if (x == NULL) printf("NULL");
    else if (x->type == &cons_type) {
        // Try to print a Scheme list
        putchar('(');
        for (;;) {
            dump(car(x));
            x = cdr(x);
            if (x && x->type == &cons_type) putchar(' ');
            else break;
        }
        if (x) {
            printf(" . ");
            dump(x);
        }
        putchar(')');
    }
    else {
        #define PRINTTYPE(t, f, fmt) if (x->type == &t) printf(fmt, x->f)
        PRINTTYPE(string_type, as_chars, "\"%s\"");
        else PRINTTYPE(symbol_type, as_chars, strchr(x->as_chars, ' ') ? "#|%s|" : "%s");
        else PRINTTYPE(integer_type, as_big_int, "%" PRId64);
        else PRINTTYPE(float_type, as_double, "%lg");
        else PRINTTYPE(c_function_type, as_ptr, "<function %p>");
        else printf("<%s:%p>", x->type->name, x->as_ptr);
        #undef PRINTTYPE
    }
}

}
